#include "Logging.hpp"

#include <DbgHelp.h>

namespace
{
    constexpr std::array<std::uint8_t, 16> SetCooperativeLevelBytes = {
        0x48, 0x8B, 0x4F, 0x70,             // MOV  RCX, QWORD PTR [RDI+70h] IDirectInputDevice8
        0x41, 0xB8, 0x15, 0x00, 0x00, 0x00, // MOV  R8D, 15h                 dwFlags
        0x48, 0x8B, 0xD0,                   // MOV  RDX, RAX                 hWnd
        0xFF, 0x53, 0x68,                   // CALL QWORD PTR [RBX+68h]      IDirectInputDevice8::SetCooperativeLevel
    };

    constexpr std::array<std::uint8_t, 3> ZeroOutRDX = {
        0x48, 0x31, 0xD2,                   // XOR  RDX, RDX
    };

    constexpr std::size_t LoadHWndIntoRDXOffset = 10;

    enum class PatchResult
    {
        ErrorTargetBytesNotFound = -3,
        ErrorTextSectionNotFound = -2,
        ErrorModuleNotFound = -1,
        Success = 0,
    };

    PatchResult PatchDirectInputDevice()
    {
        const HMODULE module = GetModuleHandleW(L"SkyrimSE.exe");
        if (!module)
        {
            return PatchResult::ErrorModuleNotFound;
        }

        const PIMAGE_NT_HEADERS64 header = ImageNtHeader(module);
        const WORD sectionCount = header->FileHeader.NumberOfSections;
        const PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(header);
        const DWORD entryPointAddress = header->OptionalHeader.AddressOfEntryPoint;

        PIMAGE_SECTION_HEADER textSection = nullptr;
        if (
            auto begin = firstSection,
                 end   = firstSection + sectionCount,
                 it    = std::find_if(begin, end, [entryPointAddress](const IMAGE_SECTION_HEADER &section)
                 {
                     return section.VirtualAddress <= entryPointAddress
                         && entryPointAddress < (section.VirtualAddress + section.Misc.VirtualSize);
                 });
            it != end
        )
        {
            textSection = it;
        }
        else
        {
            return PatchResult::ErrorTextSectionNotFound;
        }

        const DWORD textSize = textSection->Misc.VirtualSize;
        const DWORD textOffset = header->OptionalHeader.BaseOfCode;
        std::uint8_t *const imageBase = reinterpret_cast<std::uint8_t *>(header->OptionalHeader.ImageBase);
        std::uint8_t *const text = imageBase + textOffset;

        std::uint8_t *target = nullptr;
        if (
            auto begin = text,
                 end   = text + textSize,
                 it    = std::search(begin, end, SetCooperativeLevelBytes.begin(), SetCooperativeLevelBytes.end());
            it != end
        )
        {
            target = it;
        }
        else
        {
            return PatchResult::ErrorTargetBytesNotFound;
        }

        std::uint8_t *const loadHWndBytes = target + LoadHWndIntoRDXOffset;
        DWORD oldProtect = 0;
        VirtualProtect(loadHWndBytes, ZeroOutRDX.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
        std::memcpy(loadHWndBytes, ZeroOutRDX.data(), ZeroOutRDX.size());
        const DWORD originalProtect = oldProtect;
        VirtualProtect(loadHWndBytes, ZeroOutRDX.size(), originalProtect, &oldProtect);

        return PatchResult::Success;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse)
{
#ifndef NDEBUG
    while (!IsDebuggerPresent())
    {
    }
#endif

    Logging::Init();

    const auto *plugin = SKSE::PluginDeclaration::GetSingleton();
    const REL::Version version = plugin->GetVersion();
    SKSE::log::info("{} v{}", plugin->GetName(), version.string("."));

    SKSE::Init(skse);

    const PatchResult result = PatchDirectInputDevice();
    switch (result)
    {
    case PatchResult::ErrorTargetBytesNotFound:
        SKSE::log::error("Failed to find section of code to be patched in executable!");
        break;
    case PatchResult::ErrorTextSectionNotFound:
        SKSE::log::error("Failed to find text section in executable!");
        break;
    case PatchResult::ErrorModuleNotFound:
        SKSE::log::error("Failed to find SkyrimSE.exe module!");
        break;
    case PatchResult::Success:
        break;
    }

    if (result != PatchResult::Success)
    {
        return false;
    }

    SKSE::log::info("{} loaded.", plugin->GetName());

    return true;
}
