#include "types.hpp"
#include "3gx.hpp"
#include "ElfConvert.hpp"
#include <yaml.h>
#include "cxxopts.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>

static bool                 g_silentMode = false;
static bool                 g_discardSymbols = false;
static std::string          g_author;
static std::string          g_title;
static std::string          g_summary;
static std::string          g_description;
static std::vector<u32>     g_targets;
std::string                 g_enclibpath{""};

#define MAKE_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))

void    CheckOptions(int& argc, const char **argv)
{
    cxxopts::Options options(argv[0], "");

    options.add_options()
        ("d,discard-symbols", "Don't include the symbols in the file")
        ("s,silent", "Don't display the text (except errors)")
        ("e,enclib", "Encryption shared library", cxxopts::value<std::string>())
        ("h,help", "Print help");

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
      std::cout <<  " - Builds plugin files to be used by Luma3DS\n" \
                    "Usage:\n"
                <<  argv[0] << " [OPTION...] <input.bin> <settings.plgInfo> <output.3gx>" << std::endl;
      exit(0);
    }

    g_silentMode = result.count("silent");
    g_discardSymbols = result.count("discard-symbols");
    
    if (result.count("enclib"))
        g_enclibpath = result["enclib"].as<std::string>();
}

u32     GetVersion(YAML::Node& settings)
{
    u32 major = 0, minor = 0, revision = 0;

    if (settings["Version"])
    {
        auto map = settings["Version"];

        if (map["Major"]) major = map["Major"].as<u32>();
        if (map["Minor"]) minor = map["Minor"].as<u32>();
        if (map["Revision"]) revision = map["Revision"].as<u32>();
    }

    return MAKE_VERSION(major, minor, revision);
}

void    GetInfos(YAML::Node& settings)
{
    if (settings["Author"])
        g_author = settings["Author"].as<std::string>();

    if (settings["Title"])
        g_title = settings["Title"].as<std::string>();

    if (settings["Summary"])
        g_summary = settings["Summary"].as<std::string>();

    if (settings["Description"])
        g_description = settings["Description"].as<std::string>();
}

void    GetTitles(YAML::Node& settings)
{
    if (settings["Targets"])
    {
        auto list =  settings["Targets"];

        if (list.size() >= 1 && list[0].as<u32>() != 0)
        {
            for (u32 i = 0; i < list.size(); ++i)
                g_targets.push_back(list[i].as<u32>());
        }
    }
}

int     main(int argc, const char **argv)
{
    int ret = 0;
    try
    {
        CheckOptions(argc, argv);

        if (!g_silentMode)
            std::cout   <<  "\n" \
                            "3DS Game eXtension Tool\n" \
                            "--------------------------\n\n";

        if (argc < 4)
        {
            if (!g_silentMode)
                std::cout   <<  " - Builds plugin files to be used by Luma3DS\n" \
                                "Usage:\n"
                            <<  argv[0] << " [OPTION...] <input.bin> <settings.plgInfo> <output.3gx>" << std::endl;
            ret = -1;
            goto exit;
        }

        _3gx_Header         header;
        ElfConvert          elfConvert(argv[1]);
        YAML::Node          settings;
        std::ifstream       settingsFile;
        std::ifstream       codeFile;
        std::ofstream       outputFile;

        // Open files
        settingsFile.open(argv[2], std::ios::in);
        outputFile.open(argv[3], std::ios::out | std::ios::trunc | std::ios::binary);

        if (!settingsFile.is_open())
        {
            std::cerr << "couldn't open: " << argv[2] << std::endl;
            ret = -1;
            goto exit;
        }

        if (!outputFile.is_open())
        {
            std::cerr << "couldn't open: " << argv[3] << std::endl;
            ret = -1;
            goto exit;
        }

        if (!g_silentMode)
            std::cout << "Processing settings..." << std::endl;

        // Parse yaml
        settings = YAML::Load(settingsFile);

        // Fetch version
        header.version = GetVersion(settings);

        // Fetch Infos
        GetInfos(settings);

        // Fetch titles
        GetTitles(settings);

        if (!g_silentMode)
            std::cout << "Creating file..." << std::endl;

        // Reserve header place
        outputFile.write((const char *)&header, sizeof(_3gx_Header));
        outputFile.flush();

        if (!g_title.empty())
        {
            header.infos.titleLen = g_title.size() + 1;
            header.infos.titleMsg = (u32)outputFile.tellp();
            outputFile << g_title << '\0';
            outputFile.flush();
        }

        if (!g_author.empty())
        {
            header.infos.authorLen = g_author.size() + 1;
            header.infos.authorMsg = (u32)outputFile.tellp();
            outputFile << g_author << '\0';
            outputFile.flush();
        }

        if (!g_summary.empty())
        {
            header.infos.summaryLen = g_summary.size() + 1;
            header.infos.summaryMsg = (u32)outputFile.tellp();
            outputFile << g_summary << '\0';
            outputFile.flush();
        }

        if (!g_description.empty())
        {
            header.infos.descriptionLen = g_description.size() + 1;
            header.infos.descriptionMsg = (u32)outputFile.tellp();
            outputFile << g_description << '\0';
            outputFile.flush();
        }

        if (!g_targets.empty())
        {
            header.targets.count = g_targets.size();
            header.targets.titles = (u32)outputFile.tellp();
            outputFile.write((const char *)g_targets.data(), 4 * g_targets.size());
            outputFile.flush();
        }

        elfConvert.WriteToFile(header, outputFile, !g_discardSymbols);

        // Write updated header to file
        outputFile.seekp(0, std::ios::beg);
        outputFile.write((const char *)&header, sizeof(_3gx_Header));
        outputFile.flush();
        codeFile.close();
        outputFile.close();

        if (!g_silentMode)
            std::cout << "Done" << std::endl;
    }
    catch (std::exception& e)
    {
        remove(argv[3]);
        std::cerr << "An exception occured: " << e.what() << std::endl;
        ret = -1;
        goto exit;
    }
exit:
    return ret;
}
