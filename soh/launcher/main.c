// Ship's executable: the game lives in the soh module so that mods and Fleet load the same
// binary. This only forwards the command line, in UTF-8, as SDL2main used to.

int Soh_RunMain(int argc, char* argv[]);

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>
#include <shellapi.h>

static char* ConvertToUtf8(const wchar_t* text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char* converted = malloc(size);
    if (converted != NULL) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, converted, size, NULL, NULL);
    }
    return converted;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
    int argc = 0;
    wchar_t** wideArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (wideArgv == NULL) {
        return 1;
    }
    char** argv = calloc(argc + 1, sizeof(char*));
    if (argv == NULL) {
        LocalFree(wideArgv);
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        argv[i] = ConvertToUtf8(wideArgv[i]);
    }
    LocalFree(wideArgv);

    int result = Soh_RunMain(argc, argv);

    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
    return result;
}
#else
int main(int argc, char* argv[]) {
    return Soh_RunMain(argc, argv);
}
#endif
