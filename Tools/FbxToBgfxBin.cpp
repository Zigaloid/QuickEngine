/// Offline command-line tool:
///   FbxToBgfxBin.exe  input.fbx  output.bin  [scale]
///
/// Converts an FBX file to the bgfx binary mesh format that meshLoad() reads.

#include "ResourceTypes/FbxMeshConverter.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::fprintf(stderr,
            "Usage: FbxToBgfxBin <input.fbx> <output.bin> [scale]\n");
        return 1;
    }

    const char* inPath  = argv[1];
    const char* outPath = argv[2];

    FbxConvertOptions opts;
    if (argc >= 4)
        opts.scaleFactor = static_cast<float>(std::atof(argv[3]));

    std::vector<uint8_t> binary;
    if (!ConvertFbxToBgfxBinary(inPath, opts, binary))
    {
        std::fprintf(stderr, "Error: failed to convert '%s'.\n", inPath);
        return 1;
    }

    std::ofstream out(outPath, std::ios::binary);
    if (!out)
    {
        std::fprintf(stderr, "Error: cannot open '%s' for writing.\n", outPath);
        return 1;
    }

    out.write(reinterpret_cast<const char*>(binary.data()),
              static_cast<std::streamsize>(binary.size()));
    out.close();

    std::printf("Wrote %zu bytes -> %s  (%zu groups)\n",
                binary.size(), outPath,
                /* quick parse */ binary.size() > 0 ? 1u : 0u);
    return 0;
}