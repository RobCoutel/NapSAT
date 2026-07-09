#pragma once

#include "SAT-types.hpp"
#include <functional>

namespace napsat::exporter
{
  class implication_graph_exporter
  {
  public:
    /**
     * @brief Exports the implication graph to an obsidian folder with the given name. The folder will be created if it does not exist.
     * @details Each variable will be a separate file in the folder, and the edges will be created using obsidian links. The files will be named after the literal in the trail, and the content of the file will be the information about the variable provided by the lit_info function.
     * @param folder_name The name of the folder to export the graph to.
     * @param assignment The current assignment of literals. This is used to determine which literals are assigned and which are not.
     * @param get_reason A function that takes a literal and returns a pointer to an array of literals that are the reason for the assignment of the given literal. This is used to determine the edges in the implication graph.
     * @param get_reason_size A function that takes a literal and returns the size of the array returned by get_reason. This is used to determine the number of edges in the implication graph.
     * @param lit_info A function that takes a literal and returns a string with information the use would like to store in the file for the given literal. This is used to provide additional information about the variables in the implication graph.
     */
    static void export_implication_graph(const std::string& folder_name,
                                         size_t nvars,
                                         const std::vector<Tlit>& assignment,
                                         std::function<const Tlit*(Tlit)> get_reason,
                                         std::function<size_t(Tlit)> get_reason_size,
                                         std::function<std::string(Tlit)> lit_info);
  private:
    /**
     * @brief Creates a folder with the given name if it does not exist. If the folder already exists, cleans it by deleting all files in it. This is used to ensure that the folder is empty before exporting the implication graph.
     */
    static void create_folder_if_not_exists(const std::string& folder_name);

    /**
     * @brief Creates a file for a literal in the given folder. The file will contain the content and links to the literals it implies..
     */
    static void create_file_for_literal(const std::string& folder_name,
                                        Tlit lit,
                                        const std::string& content,
                                        const std::vector<Tlit>& implying_literals);
  };
}
