#include <fstream>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

std::wstring string_to_wstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

bool download(std::wstring url, std::filesystem::path output) {
    std::wcout << L"Opening session..." << std::endl;
    HINTERNET hSession = WinHttpOpen(
        L"PMFF Package Manager",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        std::wcerr << L"WinHttpOpen failed with error: " << GetLastError() << std::endl;
        return false;
    }

    URL_COMPONENTS urlcomp{};
    urlcomp.dwStructSize = sizeof(urlcomp);

    wchar_t hostname[256];
    wchar_t urlpath[1024];

    urlcomp.lpszHostName = hostname;
    urlcomp.dwHostNameLength = sizeof(hostname) / sizeof(hostname[0]);

    urlcomp.lpszUrlPath = urlpath;
    urlcomp.dwUrlPathLength = sizeof(urlpath) / sizeof(urlpath[0]);

    std::wcout << L"Parsing URL..." << std::endl;
    BOOL result = WinHttpCrackUrl(
        url.c_str(),
        (DWORD)url.length(),
        0,
        &urlcomp
    );

    if (result == 0) {
        std::wcerr << L"WinHttpCrackUrl failed with error: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wcout << L"Connecting to host: " << hostname << std::endl;
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        urlcomp.lpszHostName,
        urlcomp.nPort,
        0
    );

    if (!hConnect) {
        std::wcerr << L"WinHttpConnect failed with error: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = 0;
    if (urlcomp.nScheme == INTERNET_SCHEME_HTTPS) {
        flags = WINHTTP_FLAG_SECURE;
    }

    std::wcout << L"Opening HTTP request..." << std::endl;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlcomp.lpszUrlPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        std::wcerr << L"WinHttpOpenRequest failed with error: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wcout << L"Sending request..." << std::endl;
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0)) {
        std::wcerr << L"WinHttpSendRequest failed with error: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wcout << L"Receiving response..." << std::endl;
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::wcerr << L"WinHttpReceiveResponse failed with error: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wcout << L"Opening output file: " << output.c_str() << std::endl;
    std::ofstream outfile(output, std::ios::binary);
    if (!outfile) {
        std::wcerr << L"Failed to open output file" << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            std::wcerr << L"WinHttpQueryDataAvailable failed with error: " << GetLastError() << std::endl;
            outfile.close();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (dwSize == 0)
            break;

        std::vector<char> buffer(dwSize);
        DWORD dwDownloaded = 0;
        BOOL readResult = WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);

        if (!readResult || dwDownloaded == 0) {
            std::wcerr << L"WinHttpReadData failed or no data read, error: " << GetLastError() << std::endl;
            outfile.close();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        outfile.write(buffer.data(), dwDownloaded);
        std::wcout << L"Downloaded " << dwDownloaded << L" bytes..." << std::endl;

    } while (dwSize > 0);

    outfile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::wcout << L"Download complete" << std::endl;

    return true;
}

void init(std::filesystem::path root) {
    std::filesystem::create_directories(root);
    std::filesystem::create_directories(root / "app_manifests");
    std::filesystem::create_directories(root / "apps");
    std::filesystem::create_directories(root / "cache");
    if (!std::filesystem::exists(root / "installed_apps.json")) {
        std::ofstream out_installed_apps(root / "installed_apps.json");
        out_installed_apps << "[]";
    }
}

void list(std::filesystem::path root) {
    if (!std::filesystem::exists(root / "installed_apps.json")) {
        std::cout << "No apps installed" << std::endl;
        return;
    }
    std::ifstream in_installed_apps(root / "installed_apps.json");
    json installed_apps;
    in_installed_apps >> installed_apps;
    if (installed_apps.empty()) {
        std::cout << "No apps installed" << std::endl;
        return;
    }
    for (const auto& app : installed_apps) {
        std::cout << "- " << std::string(app["name"]) << " [" << std::string(app["version"]) << "]" << std::endl;
    }
}

void install(std::string appname, std::filesystem::path root) {
    std::filesystem::path manifest_path = root / "app_manifests" / (appname + ".json");
    if (!std::filesystem::exists(manifest_path)) {
        std::cout << "App not found" << std::endl;
        return;
    }
    std::ifstream in_manifest(manifest_path);
    json manifest;
    in_manifest >> manifest;
    std::ifstream in_installed_apps(root / "installed_apps.json");
    json installed_apps;
    in_installed_apps >> installed_apps;

    for (size_t i = 0; i < installed_apps.size(); i++) {
        if (installed_apps[i]["name"] == manifest["name"]) {
            std::cout << "App already installed" << std::endl;
            if (installed_apps[i]["version"] != manifest["version"]) {
                std::cout << "Another version of " << std::string(installed_apps[i]["name"]) << " is available. Install new version? (Y/n): ";
                std::string input;
                std::getline(std::cin, input);
                if (!input.empty() && (input[0] == 'n' || input[0] == 'N')) {
                    std::cout << "Keeping old version" << std::endl;
                    return;
                }
                std::string url = manifest["url"];
                std::wstring wide_url = string_to_wstring(url);

                size_t last_dot = url.find_last_of('.');
                size_t last_slash = url.find_last_of('/');
                std::string ext = ".zip";
                if (last_dot != std::string::npos && (last_slash == std::string::npos || last_dot > last_slash)) {
                    ext = url.substr(last_dot);
                }

                if (ext != ".zip") {
                    std::cout << "Only .zip files supported for installation" << std::endl;
                    return;
                }

                std::filesystem::path cache_path = root / "cache" / (appname + "-" + std::string(manifest["version"]) + ext);

                if (!download(wide_url, cache_path)) {
                    std::cout << "Download failed" << std::endl;
                    return;
                }

                std::filesystem::path install_dir = root / "apps" / appname;
                if (std::filesystem::exists(install_dir)) {
                    std::filesystem::remove_all(install_dir);
                }

                std::string command = "powershell -Command \"Expand-Archive -Path '" +
                                      cache_path.string() +
                                      "' -DestinationPath '" +
                                      install_dir.string() +
                                      "' -Force\"";
                std::system(command.c_str());

                installed_apps[i] = manifest;
                std::ofstream out_installed_apps(root / "installed_apps.json");
                out_installed_apps << installed_apps.dump(4);
                std::cout << "Updated to new version" << std::endl;
                return;
            }
            return;
        }
    }

    std::string url = manifest["url"];
    std::wstring wide_url = string_to_wstring(url);

    size_t last_dot = url.find_last_of('.');
    size_t last_slash = url.find_last_of('/');
    std::string ext = ".zip";
    if (last_dot != std::string::npos && (last_slash == std::string::npos || last_dot > last_slash)) {
        ext = url.substr(last_dot);
    }

    if (ext != ".zip") {
        std::cout << "Only .zip files supported for installation." << std::endl;
        return;
    }

    std::filesystem::path cache_path = root / "cache" / (appname + "-" + std::string(manifest["version"]) + ext);

    if (!download(wide_url, cache_path)) {
        std::cout << "Download failed" << std::endl;
        return;
    }

    std::filesystem::path install_dir = root / "apps" / appname;
    if (std::filesystem::exists(install_dir)) {
        std::filesystem::remove_all(install_dir);
    }

    std::string command = "powershell -Command \"Expand-Archive -Path '" +
                          cache_path.string() +
                          "' -DestinationPath '" +
                          install_dir.string() +
                          "' -Force\"";
    std::system(command.c_str());

    installed_apps.push_back(manifest);
    std::ofstream out_installed_apps(root / "installed_apps.json");
    out_installed_apps << installed_apps.dump(4);
    std::cout << "Installed successfully" << std::endl;
}

void remove(std::string appname, std::filesystem::path root) {
    std::ifstream in_installed_apps(root / "installed_apps.json");
    json installed_apps;
    in_installed_apps >> installed_apps;
    for (size_t i = 0; i < installed_apps.size(); i++) {
        if (installed_apps[i]["name"] == appname) {
            std::cout << "Removing " << appname << "..." << std::endl;
            std::filesystem::remove_all(root / "apps" / appname);
            std::cout << "Uninstall completed successfully" << std::endl;
            installed_apps.erase(installed_apps.begin() + i);
            std::ofstream out_installed_apps(root / "installed_apps.json");
            out_installed_apps << installed_apps.dump(4);
            return;
        }
    }
    std::cout << "No app with the matching name installed" << std::endl;
}

void create(std::string url, std::filesystem::path root) {
    json manifest;
    std::string input;

    std::cout << "Input name: ";
    std::getline(std::cin, input);
    manifest["name"] = input;

    if (std::filesystem::exists(root / "app_manifests" / (std::string(manifest["name"]) + ".json"))) {
        std::cout << "Manifest already exists" << std::endl;
        return;
    }

    std::cout << "Input version: ";
    std::getline(std::cin, input);
    manifest["version"] = input;

    manifest["url"] = url;

    std::ofstream out_manifest(root / "app_manifests" / (std::string(manifest["name"]) + ".json"));
    out_manifest << manifest.dump(4);

    std::cout << "Created manifest for " << std::string(manifest["name"]) << std::endl;
}

int main(int argc, char** argv) {
    const char* path = std::getenv("LOCALAPPDATA");
    if (path == nullptr) {
        std::cout << "Failed to get LOCALAPPDATA env" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::filesystem::path pmffroot = std::string(path) + "\\pmff";
    if (!std::filesystem::exists(pmffroot)) init(pmffroot);

    if (argc < 2) {
        std::cout << "No arguments provided. Run 'pmff help' for a list of commands" << std::endl;
        return 0;
    }

    if (std::string(argv[1]) == "help") {
        std::cout << "pmff list: lists installed apps" << std::endl;
        std::cout << "pmff path: outputs pmff's path" << std::endl;
        std::cout << "pmff install <appname>: installs an app" << std::endl;
        std::cout << "pmff remove <appname>: uninstalls an app" << std::endl;
        std::cout << "pmff create <url>: creates a manifest for an app" << std::endl;
    } else if (std::string(argv[1]) == "list") {
        list(pmffroot);
    } else if (std::string(argv[1]) == "path") {
        std::string path = pmffroot.string();
        std::replace(path.begin(), path.end(), '\\', '/');
        std::cout << path << std::endl;
    } else if (std::string(argv[1]) == "install") {
        if (argc < 3) {
            std::cout << "No app specified" << std::endl;
            return 0;
        }
        install(std::string(argv[2]), pmffroot);
    } else if (std::string(argv[1]) == "remove") {
        if (argc < 3) {
            std::cout << "No app specified" << std::endl;
            return 0;
        }
        remove(std::string(argv[2]), pmffroot);
    } else if (std::string(argv[1]) == "create") {
        if (argc < 3) {
            std::cout << "No URL provided" << std::endl;
            return 0;
        }
        create(std::string(argv[2]), pmffroot);
    }

    return 0;
}
