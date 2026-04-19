#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <string>

static const int CHUNK_WIDTH  = 16;
static const int CHUNK_HEIGHT = 256;
static const int BYTES_PER_BLOCK = 5; // 4 (BlockID) + 1 (metadata)

// ANSI color codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define GRAY    "\033[90m"
#define BBLUE   "\033[94m"
#define BGREEN  "\033[92m"
#define BYELLOW "\033[93m"
#define BWHITE  "\033[97m"

struct BlockEntry {
    char symbol;
    const char* color;
    const char* name;
};

// Indexed by BlockID (0..12)
static const BlockEntry BLOCKS[] = {
    { ' ', RESET,    "Air"         },  // 0
    { 'S', GRAY,     "Stone"       },  // 1
    { 'T', BGREEN,   "Grass"       },  // 2
    { 'Z', YELLOW,   "Dirt"        },  // 3
    { 'C', WHITE,    "Cobblestone" },  // 4
    { 'O', MAGENTA,  "Obsidian"    },  // 5
    { 'B', GRAY,     "Bedrock"     },  // 6
    { 'W', BBLUE,    "Water"       },  // 7
    { 'I', WHITE,    "Iron Ore"    },  // 8
    { 'G', BYELLOW,  "Gold Ore"    },  // 9
    { 'D', CYAN,     "Diamond Ore" },  // 10
    { 'L', YELLOW,   "Oak Log"     },  // 11
    { 'F', GREEN,    "Oak Leaves"  },  // 12
};
static const int KNOWN_BLOCKS = (int)(sizeof(BLOCKS) / sizeof(BLOCKS[0]));

static void printLegend() {
    std::cout << BOLD << "=== LEGENDA ===" << RESET << "\n";
    for (int i = 0; i < KNOWN_BLOCKS; i++) {
        if (BLOCKS[i].symbol == ' ') continue;
        std::cout << "  " << BLOCKS[i].color << BOLD
                  << BLOCKS[i].symbol << RESET
                  << "  " << BLOCKS[i].name << " (id=" << i << ")\n";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uzycie: " << argv[0] << " <plik_chunku> [min_y] [max_y]\n";
        std::cerr << "Przyklad: " << argv[0] << " ../saves/Swiat/chunk_8\n";
        std::cerr << "          " << argv[0] << " ../saves/Swiat/chunk_8 60 80\n";
        return 1;
    }

    const char* path = argv[1];
    int y_min = 0;
    int y_max = CHUNK_HEIGHT - 1;

    if (argc >= 3) y_min = std::stoi(argv[2]);
    if (argc >= 4) y_max = std::stoi(argv[3]);

    if (y_min < 0) y_min = 0;
    if (y_max >= CHUNK_HEIGHT) y_max = CHUNK_HEIGHT - 1;
    if (y_min > y_max) std::swap(y_min, y_max);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Blad: nie mozna otworzyc pliku: " << path << "\n";
        return 1;
    }

    // Read entire chunk into memory
    static uint32_t blockIDs[CHUNK_HEIGHT][CHUNK_WIDTH];
    static uint8_t  metadata[CHUNK_HEIGHT][CHUNK_WIDTH];

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            uint32_t id = 0;
            uint8_t  meta = 0;
            file.read(reinterpret_cast<char*>(&id),   sizeof(id));
            file.read(reinterpret_cast<char*>(&meta), sizeof(meta));
            blockIDs[y][x] = id;
            metadata[y][x] = meta;
        }
    }
    file.close();

    printLegend();

    std::cout << BOLD << "Plik: " << path << RESET << "\n";
    std::cout << BOLD << "Zakres Y: " << y_min << " - " << y_max << RESET << "\n\n";

    // Header: x indices
    std::cout << "    ";
    for (int x = 0; x < CHUNK_WIDTH; x++)
        std::cout << (x % 10);
    std::cout << "\n";
    std::cout << "    ";
    for (int x = 0; x < CHUNK_WIDTH; x++)
        std::cout << "-";
    std::cout << "\n";

    // Rows from top (high y) to bottom (low y)
    for (int y = y_max; y >= y_min; y--) {
        std::cout << BOLD << (y < 100 ? " " : "") << (y < 10 ? " " : "") << y << "|" << RESET;
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            uint32_t id = blockIDs[y][x];
            if (id < (uint32_t)KNOWN_BLOCKS) {
                const BlockEntry& b = BLOCKS[id];
                if (b.symbol == ' ')
                    std::cout << ' ';
                else
                    std::cout << b.color << BOLD << b.symbol << RESET;
            } else {
                // Unknown block - print '?' in red
                std::cout << RED << '?' << RESET;
            }
        }
        std::cout << "\n";
    }

    return 0;
}
