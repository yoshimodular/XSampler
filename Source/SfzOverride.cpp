#include "SfzOverride.h"
#include <regex>
#include <unordered_set>

namespace
{
    const char* sfzFilterType (int t)
    {
        switch (t)
        {
            case 1: return "hpf_2p";
            case 2: return "bpf_2p";
            default: return "lpf_2p";
        }
    }

    int sfzLfoWave (int idx)
    {
        switch (idx)
        {
            case 0: return 1; // Sine
            case 1: return 0; // Tri
            case 2: return 7; // Saw
            case 3: return 3; // Square
            case 4: return 8; // Stepped
            default: return 1;
        }
    }

    const char* lfoTargetOpcode (int t)
    {
        switch (t)
        {
            case 1: return "lfo99_cutoff_oncc";
            case 2: return "lfo99_volume_oncc";
            default: return "lfo99_pitch_oncc";
        }
    }

    int lfoTargetMaxDepth (int t)
    {
        switch (t)
        {
            case 1: return 4800;
            case 2: return 24;
            default: return 1200;
        }
    }

    // Opcodes we ALWAYS strip from the user's source — they're mapped to
    // a macro knob and stripping is essential so per-region values don't
    // shadow our <global> CC routing.
    //
    // We deliberately DO NOT strip these (they're often authored
    // intentionally and we don't necessarily replace them):
    //   * `trigger`  — only stripped when our own legato wraps regions
    //   * `offset`   — user's deliberate offsets stay; our CC adds on top
    //   * `pan`      — only stripped when our doubler wraps regions
    //   * `tune`     — kept; users rarely set per-region tune, and we
    //                   apply ours via tune_oncc on top of any existing.
    // Lists are now CONDITIONAL — applied only when the corresponding
    // overlay section is active. This keeps audio bit-exact to vanilla
    // when knobs are at their defaults.
    juce::StringArray ampegStrip()      { return { "ampeg_attack", "ampeg_decay",
                                                   "ampeg_sustain", "ampeg_release" }; }
    juce::StringArray filterStrip()     { return { "cutoff", "resonance", "fil_type",
                                                   "fileg_attack", "fileg_decay",
                                                   "fileg_sustain", "fileg_release", "fileg_depth" }; }
    juce::StringArray ampVelTrackStrip(){ return { "amp_veltrack" }; }
    juce::StringArray filVelTrackStrip(){ return { "fil_veltrack" }; }
    juce::StringArray analogStrip()     { return { "pitch_random", "delay_random" }; }
    juce::StringArray sampleStartStrip(){ return { "offset" }; }
    juce::StringArray tuneStrip()       { return { "tune" }; }
    // Mono mode always strips polyphony when active.
    juce::StringArray polyphonyStrip()  { return { "polyphony" }; }

    juce::String stripOpcode (const juce::String& text, const juce::String& name)
    {
        const std::regex re (
            "(^|[\\s\\t])" + name.toStdString() + "(_[a-z0-9_]+)?=\\S+",
            std::regex_constants::icase);
        return juce::String (std::regex_replace (text.toStdString(), re, "$1"));
    }

    // Strip ONLY our reserved LFO index (lfo99) from the user's source.
    // The user's own lfo01..lfo32 (vibrato, tremolo, modulation routing)
    // stays intact — many real-world banks rely on those for their
    // character (the Aliexpress Erhu vibrato uses lfo01/03/05/06).
    juce::String stripOurLfoOpcodes (const juce::String& text)
    {
        const std::regex re ("(^|[\\s\\t])lfo99_[a-z0-9_]*=\\S+",
                             std::regex_constants::icase);
        return juce::String (std::regex_replace (text.toStdString(), re, "$1"));
    }

    // Index of the first SFZ region/group/master/global header (NOT control).
    // Our overlay's <global> block is prepended here so any `<control>` that
    // the user authored at the top of the file stays in the preamble where
    // sfizz expects it.
    int firstNonControlHeader (const juce::String& text)
    {
        int best = -1;
        for (auto h : { "<region>", "<group>", "<master>", "<global>" })
        {
            int idx = text.indexOf (h);
            if (idx >= 0 && (best < 0 || idx < best)) best = idx;
        }
        return best;
    }

    int firstRegionHeader (const juce::String& text)
    {
        int best = -1;
        for (auto h : { "<region>", "<group>", "<master>" })
        {
            int idx = text.indexOf (h);
            if (idx >= 0 && (best < 0 || idx < best)) best = idx;
        }
        return best;
    }

    // Inline every #include directive recursively so the strip pass sees
    // the full effective SFZ source. Without this, opcodes inside included
    // map files would shadow our <global> (per-region wins over global)
    // and our macro knobs would have no audible effect. Banks like the
    // Aliexpress Erhu legitimately include the same map file from several
    // <master> blocks — so we DON'T deduplicate, only cap recursion depth.
    // Walk the file, tracking the active <control> default_path, and
    // rewrite every `sample=PATH` opcode to an absolute path. Resolution
    // order:
    //   1) baseDir / default_path / PATH         (include-relative)
    //   2) topDir  / default_path / PATH         (top-level relative)
    // The first one that exists wins; if neither does, we still emit
    // the include-relative absolute (sfizz will report the missing file).
    // SFZ values for `sample=` can legitimately contain spaces (e.g.
    // `Resonant2 Samples/foo.flac`), so we can't stop at the next
    // whitespace. Instead we capture everything up to either EOL or the
    // next opcode keyword on the same line — `<space>identifier=`.
    juce::String absolutiseSamplePaths (const juce::String& text,
                                        const juce::File& baseDir,
                                        const juce::File& topDir)
    {
        juce::String defaultPath;  // accumulated default_path
        juce::StringArray lines = juce::StringArray::fromLines (text);

        auto extractValueAndTail = [] (const juce::String& after) -> std::pair<juce::String, juce::String>
        {
            // Walk forward looking for the next ` <ident>=` start.
            const int n = after.length();
            for (int i = 1; i < n; ++i)
            {
                if (juce::CharacterFunctions::isWhitespace (after[i - 1]))
                {
                    int j = i;
                    while (j < n && (juce::CharacterFunctions::isLetterOrDigit (after[j]) || after[j] == '_'))
                        ++j;
                    if (j < n && j > i && after[j] == '=')
                        return { after.substring (0, i - 1).trimEnd(), after.substring (i - 1) };
                }
            }
            return { after.trimEnd(), juce::String() };
        };

        for (auto& line : lines)
        {
            // Find every default_path= and sample= occurrence in the line.
            int searchFrom = 0;
            while (searchFrom < line.length())
            {
                int posDP = line.indexOfIgnoreCase (searchFrom, "default_path=");
                int posSA = line.indexOfIgnoreCase (searchFrom, "sample=");
                int pos   = (posDP >= 0 && (posSA < 0 || posDP < posSA)) ? posDP : posSA;
                if (pos < 0) break;

                // Make sure it's at start-of-line or preceded by whitespace
                // / `<>` (so we don't match `amp_sample=…` or similar).
                if (pos > 0)
                {
                    const juce::juce_wchar prev = line[pos - 1];
                    if (! (juce::CharacterFunctions::isWhitespace (prev)
                           || prev == '>'))
                    {
                        searchFrom = pos + 1;
                        continue;
                    }
                }

                const bool isSample = (pos == posSA && (posDP < 0 || posSA < posDP));
                const juce::String key = isSample ? "sample" : "default_path";
                const int valueStart   = pos + key.length() + 1;  // skip "key="
                const auto [val0, tail] = extractValueAndTail (line.substring (valueStart));
                juce::String val = val0.replaceCharacter ('\\', '/');

                juce::String replacement;
                if (! isSample)
                {
                    defaultPath = val;
                    if (defaultPath.isNotEmpty() && ! defaultPath.endsWith ("/"))
                        defaultPath << "/";
                    replacement = "// [absorbed default_path=" + val + "]";
                }
                else
                {
                    // sfizz built-in oscillators / generators (*sine, *saw,
                    // *square, *triangle, *noise, *silence) start with '*'
                    // and must NOT be absolutised — they're not files.
                    if (val.startsWithChar ('*'))
                    {
                        replacement = "sample=" + val;
                    }
                    else
                    {
                        juce::File resolved;
                        if (juce::File::isAbsolutePath (val))
                        {
                            resolved = juce::File (val);
                        }
                        else
                        {
                            auto tryResolve = [&] (const juce::File& root) {
                                juce::File r = root;
                                if (defaultPath.isNotEmpty())
                                    r = r.getChildFile (defaultPath);
                                return r.getChildFile (val);
                            };
                            juce::File a = tryResolve (baseDir);
                            juce::File b = tryResolve (topDir);
                            if (a.existsAsFile())      resolved = a;
                            else if (b.existsAsFile()) resolved = b;
                            else                       resolved = a;
                        }
                        replacement = "sample=" + resolved.getFullPathName();
                    }
                }

                // Splice replacement into the line.
                const int matchEnd = valueStart + val0.length();
                juce::String newLine = line.substring (0, pos) + replacement;
                if (tail.isNotEmpty()) newLine << tail;
                else newLine << line.substring (matchEnd);

                line = newLine;
                searchFrom = pos + replacement.length();
            }
        }

        return lines.joinIntoString ("\n");
    }

    // `pathStack` carries the chain of currently-open files. A re-entry
    // means the include path is cyclic and we drop the directive (replaced
    // with a comment) so sfizz never sees it again. Sibling re-includes
    // are fine because the prior occurrence has already left the stack.
    juce::String inlineIncludes (const juce::String& text,
                                 const juce::File& baseFile,
                                 const juce::File& topFile,
                                 std::unordered_set<std::string>& pathStack,
                                 int depth = 0)
    {
        if (depth > 16) return juce::String (); // truncate; emit nothing

        const std::regex re ("(^|[\\s\\t])(#include|\\$include)[\\s\\t]+\"([^\"]+)\"");
        const std::string in = text.toStdString();
        std::string out;
        out.reserve (in.size());

        auto it  = std::sregex_iterator (in.begin(), in.end(), re);
        auto end = std::sregex_iterator();
        std::size_t lastEnd = 0;

        for (; it != end; ++it)
        {
            const auto& m = *it;
            out.append (in, lastEnd, (std::size_t) m.position() - lastEnd);

            const juce::String includePath (m[3].str());
            const juce::File inc = baseFile.getParentDirectory().getChildFile (includePath);
            if (! inc.existsAsFile())
            {
                // Drop unresolved includes — sfizz would reject the bank
                // with a missing-file error. The processor surfaces a
                // load failure in compatibility tests instead.
                out.append (m[1].str());
                out.append (" // [missing include: ");
                out.append (includePath.toStdString());
                out.append ("]\n");
            }
            else
            {
                const std::string canonical = inc.getFullPathName().toStdString();
                if (pathStack.count (canonical))
                {
                    // Cycle on the current path — break it.
                    out.append (m[1].str());
                    out.append (" // [cyclic include skipped]\n");
                }
                else
                {
                    pathStack.insert (canonical);
                    juce::String childText = inc.loadFileAsString();
                    childText = absolutiseSamplePaths (
                        childText,
                        inc.getParentDirectory(),
                        topFile.getParentDirectory());
                    const juce::String inlined = inlineIncludes (
                        childText, inc, topFile, pathStack, depth + 1);
                    pathStack.erase (canonical);

                    out.append (m[1].str());
                    out.append ("\n");
                    out.append (inlined.toStdString());
                    out.append ("\n");
                }
            }

            lastEnd = (std::size_t) m.position() + (std::size_t) m.length();
        }
        out.append (in, lastEnd, in.size() - lastEnd);
        return juce::String (out);
    }
}

// Inject `body` (a list of opcodes, no <global> header) into every
// `<global>` block in `text`. Also wraps any `<master>`/`<group>`/
// `<region>` content that appears before the FIRST `<global>` in an
// implicit `<global>` containing `body` (and any leading user opcodes
// that were sitting in default scope).
static juce::String injectIntoEveryGlobal (const juce::String& text,
                                           const juce::String& body)
{
    // Find every `<global>` header.
    juce::Array<int> positions;
    int p = 0;
    while ((p = text.indexOf (p, "<global>")) >= 0)
    {
        positions.add (p);
        p += 8;
    }

    if (positions.isEmpty())
    {
        // No <global> in user content → prepend one. Place it BEFORE any
        // <master>/<group>/<region> so subsequent regions inherit it.
        int regHdr = text.indexOf ("<region>");
        for (auto h : { "<group>", "<master>" })
        {
            int q = text.indexOf (h);
            if (q >= 0 && (regHdr < 0 || q < regHdr)) regHdr = q;
        }
        if (regHdr < 0)
            return text + "\n<global>\n" + body + "\n";

        juce::String pre  = text.substring (0, regHdr);
        juce::String post = text.substring (regHdr);
        return pre + "<global>\n" + body + "\n" + post;
    }

    juce::String out;
    int lastEnd = 0;
    for (int pos : positions)
    {
        const int afterHeader = pos + 8;
        out << text.substring (lastEnd, afterHeader) << "\n" << body << "\n";
        lastEnd = afterHeader;
    }
    out << text.substring (lastEnd);
    return out;
}

XSamplerSfzDefaults extractDefaults (const juce::File& originalSfz)
{
    XSamplerSfzDefaults out;
    if (! originalSfz.existsAsFile()) return out;

    // Inline includes so defaults from sub-files (e.g. erhu's common.sfz)
    // are visible.
    juce::String text = originalSfz.loadFileAsString();
    {
        std::unordered_set<std::string> stack;
        stack.insert (originalSfz.getFullPathName().toStdString());
        text = inlineIncludes (text, originalSfz, originalSfz, stack);
    }

    // For each opcode of interest, grab the first occurrence's value.
    auto findFirst = [&text] (const char* name) -> juce::String
    {
        const std::regex re (
            std::string ("(^|[\\s\\t])") + name + "=([^\\s\\r\\n]+)",
            std::regex_constants::icase);
        std::smatch m;
        const std::string s = text.toStdString();
        if (std::regex_search (s, m, re))
            return juce::String (m[2].str());
        return {};
    };

    auto floatVal = [&] (const char* name, float& out) {
        auto v = findFirst (name);
        if (v.isNotEmpty()) out = v.getFloatValue();
    };

    floatVal ("ampeg_attack",  out.ampegAttack);
    floatVal ("ampeg_decay",   out.ampegDecay);
    floatVal ("ampeg_sustain", out.ampegSustain);
    floatVal ("ampeg_release", out.ampegRelease);
    floatVal ("fileg_attack",  out.filegAttack);
    floatVal ("fileg_decay",   out.filegDecay);
    floatVal ("fileg_sustain", out.filegSustain);
    floatVal ("fileg_release", out.filegRelease);
    floatVal ("fileg_depth",   out.filegDepth);
    floatVal ("cutoff",        out.cutoff);
    floatVal ("resonance",     out.resonance);
    floatVal ("amp_veltrack",  out.ampVelTrack);
    floatVal ("fil_veltrack",  out.filVelTrack);

    auto filTypeStr = findFirst ("fil_type");
    if (filTypeStr.isNotEmpty())
    {
        if      (filTypeStr.startsWith ("lpf")) out.filType = 0;
        else if (filTypeStr.startsWith ("hpf")) out.filType = 1;
        else if (filTypeStr.startsWith ("bpf")) out.filType = 2;
    }
    return out;
}

juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p)
{
    if (! originalSfz.existsAsFile())
        return {};

    juce::String original = originalSfz.loadFileAsString();

    const juce::File topDir = originalSfz.getParentDirectory();
    original = absolutiseSamplePaths (original, topDir, topDir);
    {
        std::unordered_set<std::string> pathStack;
        pathStack.insert (originalSfz.getFullPathName().toStdString());
        original = inlineIncludes (original, originalSfz, originalSfz, pathStack);
    }

    auto stripList = [&] (const juce::StringArray& names) {
        for (const auto& op : names) original = stripOpcode (original, op);
    };

    if (p.ampegActive)       stripList (ampegStrip());
    if (p.filterActive)      stripList (filterStrip());
    if (p.ampVelTrackActive) stripList (ampVelTrackStrip());
    if (p.filVelTrackActive) stripList (filVelTrackStrip());
    if (p.analogActive)      stripList (analogStrip());
    if (p.sampleStartActive) stripList (sampleStartStrip());
    if (p.tuneActive)        stripList (tuneStrip());
    if (p.mono)              stripList (polyphonyStrip());
    original = stripOurLfoOpcodes (original);

    if (p.legato && p.mono)
        original = stripOpcode (original, "trigger");
    if (p.doubler)
        original = stripOpcode (original, "pan");

    // Build our overlay opcode body (NO <global> header — we inject this
    // into every user <global> block to survive global-scope resets).
    juce::String g;
    g << "// XSampler runtime overlay\n";

    if (p.mono)
        g << "polyphony=1\n";

    if (p.tuneActive)
        g << "tune=-100\n"
          << "tune_oncc" << XSamplerCC::Tune << "=200\n";

    if (p.filterActive)
    {
        g << "bend_up=2400\nbend_down=-2400\n";  // wider bend if filter active
        g << "fil_type="     << sfzFilterType (p.filterType) << "\n"
          << "cutoff=20\n"
          << "cutoff_oncc"   << XSamplerCC::Cutoff     << "=12000\n"
          << "resonance=0\n"
          << "resonance_oncc"<< XSamplerCC::Resonance  << "=24\n"
          << "fileg_attack=0\n"  << "fileg_attack_oncc"  << XSamplerCC::FltAttack  << "=10\n"
          << "fileg_decay=0\n"   << "fileg_decay_oncc"   << XSamplerCC::FltDecay   << "=10\n"
          << "fileg_sustain=0\n" << "fileg_sustain_oncc" << XSamplerCC::FltSustain << "=100\n"
          << "fileg_release=0\n" << "fileg_release_oncc" << XSamplerCC::FltRelease << "=20\n"
          << "fileg_depth=-4800\n"
          << "fileg_depth_oncc"  << XSamplerCC::FltEnvAmount << "=9600\n";
    }

    if (p.ampegActive)
        g << "ampeg_attack=0\n"  << "ampeg_attack_oncc"  << XSamplerCC::VolAttack  << "=10\n"
          << "ampeg_decay=0\n"   << "ampeg_decay_oncc"   << XSamplerCC::VolDecay   << "=10\n"
          << "ampeg_sustain=0\n" << "ampeg_sustain_oncc" << XSamplerCC::VolSustain << "=100\n"
          << "ampeg_release=0\n" << "ampeg_release_oncc" << XSamplerCC::VolRelease << "=20\n";

    if (p.lfoActive)
    {
        g << "lfo99_freq=0\n"
          << "lfo99_freq_oncc"  << XSamplerCC::LfoRate << "=20\n"
          << "lfo99_delay=0\n"
          << "lfo99_delay_oncc" << XSamplerCC::LfoDelay << "=4\n"
          << "lfo99_wave="      << sfzLfoWave (p.lfoWave) << "\n"
          << lfoTargetOpcode (p.lfoTarget)
          << XSamplerCC::LfoDepth << "="
          << lfoTargetMaxDepth (p.lfoTarget) << "\n";
    }

    if (p.ampVelTrackActive)
        g << "amp_veltrack=0\n"
          << "amp_veltrack_oncc" << XSamplerCC::AmpVelTrack << "=100\n";
    if (p.filVelTrackActive)
        g << "fil_veltrack=0\n"
          << "fil_veltrack_oncc" << XSamplerCC::FilVelTrack << "=4800\n";

    if (p.analogActive)
        g << "pitch_random_oncc" << XSamplerCC::PitchRandom << "=10\n"
          << "delay_random_oncc" << XSamplerCC::DelayRandom << "=0.003\n";

    if (p.sampleStartActive)
        g << "offset_oncc" << XSamplerCC::SampleStart << "=4410\n";

    g << "\n";

    juce::String result;

    // Legato / doubler still wrap regions, so do that BEFORE injecting our
    // opcodes into <global>s.
    if (p.legato && p.mono)
    {
        const int rPos = firstRegionHeader (original);
        if (rPos < 0)
        {
            result = "trigger=legato\n" + original;
        }
        else
        {
            const juce::String beforeRegions = original.substring (0, rPos);
            const juce::String regionBlock   = original.substring (rPos);
            result << beforeRegions
                   << "<group>\ntrigger=first\n"  << regionBlock << "\n"
                   << "<group>\ntrigger=legato\n" << regionBlock << "\n";
        }
    }
    else if (p.doubler)
    {
        const int rPos = firstRegionHeader (original);
        if (rPos < 0)
        {
            result = original;
        }
        else
        {
            const juce::String beforeRegions = original.substring (0, rPos);
            const juce::String regionBlock   = original.substring (rPos);
            result << beforeRegions
                   << "<group>\npan=-100\ntune_oncc"  << XSamplerCC::PitchRandom << "=-25\n"
                   << regionBlock << "\n"
                   << "<group>\npan=100\ntune_oncc"   << XSamplerCC::PitchRandom << "=25\n"
                   << "delay_oncc"                    << XSamplerCC::DelayRandom << "=0.005\n"
                   << regionBlock << "\n";
        }
    }
    else
    {
        result = original;
    }

    // Now inject our overlay body into every <global> in the result so
    // we override every time the user's content opens a fresh global
    // scope (which would otherwise reset our opcodes).
    return injectIntoEveryGlobal (result, g);
}
