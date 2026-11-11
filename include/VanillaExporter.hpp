#ifndef VANILLA_EXPORTER_HPP
#define VANILLA_EXPORTER_HPP

#include <filesystem>
#include <string>

// One-shot dumper: iterates the live blockDatabase and itemDatabase and
// writes one JSON file per entry into the given pack directory. Overwrites
// existing files. Returns a short status string (number of files written);
// terminates the program with nonzero exit if writing fails.
//
// Triggered by setting the BLOCKBIT_EXPORT_VANILLA env var to a target
// directory before startup — the hook lives in registerVanillaContent().
namespace VanillaExporter
{

struct ExportStats
{
    std::size_t blocks_written = 0;
    std::size_t items_written  = 0;
};

ExportStats exportTo(const std::filesystem::path& dir);

} // namespace VanillaExporter

#endif // VANILLA_EXPORTER_HPP
