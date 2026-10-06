#pragma once

void caseHdrConfig()
{
    const std::filesystem::path directory = freshSaveDirectory("hdr_pipeline_config");
    const auto path = directory / "config.txt";
    const auto read = [&]() {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const auto write = [&](const std::string& text) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    };
    Config config = loadRuntimeConfig(path.string());
    check("HDR/default-legacy", config.renderPipeline == RenderPipeline::Legacy &&
          read().find("renderpipeline legacy\n") != std::string::npos);
    config.renderPipeline = RenderPipeline::LinearHdr;
    config.postProcessingQuality = PostProcessingQuality::Off;
    check("HDR/save-requested-mode", saveRuntimeConfig(path.string(), config));
    const std::string valid = read();
    check("HDR/version12-explicit-mode", valid.find("settings_version 12\n") == 0 &&
          valid.find("renderpipeline linear-hdr\n") != std::string::npos);
    const Config restored = loadRuntimeConfig(path.string());
    check("HDR/mode-and-post-off-roundtrip", restored.renderPipeline == RenderPipeline::LinearHdr &&
          restored.postProcessingQuality == PostProcessingQuality::Off);
    RuntimeSettingsSession session;
    UserSettings original;
    session.begin(original);
    session.draft().renderPipeline = RenderPipeline::LinearHdr;
    RuntimeSettingsApplyPlan plan;
    std::string error;
    check("HDR/requires-restart-without-changing-post", session.prepareApply(plan, error) &&
          plan.restartRequired && !plan.postProcessingQualityChanged &&
          plan.settings.renderPipeline == RenderPipeline::LinearHdr);
    session.cancel();
    check("HDR/cancel-closes-draft", !session.isOpen());

    std::string legacy = valid;
    legacy.replace(0, std::string("settings_version 12").size(), "settings_version 11");
    const auto field = legacy.find("renderpipeline linear-hdr\n");
    legacy.erase(field, std::string("renderpipeline linear-hdr\n").size());
    write(legacy);
    const Config migrated = loadRuntimeConfig(path.string());
    check("HDR/version11-migrates-legacy", migrated.renderPipeline == RenderPipeline::Legacy &&
          read().find("settings_version 12\n") == 0);

    const auto rejects = [&](const char* label, std::string text) {
        write(text);
        bool rejected = false;
        try { (void)loadRuntimeConfig(path.string()); }
        catch (const std::runtime_error&) { rejected = true; }
        check(label, rejected && read() == text);
    };
    std::string missing = valid;
    missing.erase(missing.find("renderpipeline linear-hdr\n"),
                  std::string("renderpipeline linear-hdr\n").size());
    rejects("HDR/missing-version12-mode-preserves-file", missing);
    std::string unknown = valid;
    unknown.replace(unknown.find("renderpipeline linear-hdr"),
                    std::string("renderpipeline linear-hdr").size(), "renderpipeline unknown");
    rejects("HDR/unknown-mode-preserves-file", unknown);
    rejects("HDR/duplicate-mode-preserves-file", valid + "renderpipeline legacy\n");
    rejects("HDR/old-version-rejects-new-field", legacy + "renderpipeline linear-hdr\n");
    std::string future = valid;
    future.replace(0, std::string("settings_version 12").size(), "settings_version 13");
    rejects("HDR/future-version-preserves-file", future);
    UserSettings invalid;
    invalid.renderPipeline = static_cast<RenderPipeline>(2);
    bool rejected = false;
    try { validateUserSettings(invalid); }
    catch (const std::runtime_error&) { rejected = true; }
    check("HDR/invalid-enum-rejected", rejected);
    write(valid);
}
