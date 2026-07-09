#include "implication_graph_exporter.hpp"

#include "SAT-options.hpp"
#include "../utils/printer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace std;
using namespace napsat;

namespace napsat::exporter
{

void implication_graph_exporter::export_implication_graph(const std::string& folder_name,
                                                          size_t nvars,
                                                          const std::vector<Tlit>& assignment,
                                                          std::function<const Tlit* (Tlit)> get_reason,
                                                          std::function<size_t(Tlit)> get_reason_size,
                                                          std::function<std::string(Tlit)> lit_info)
{
  std::string actual_folder_name = folder_name;

  LOG_INFO("Exporting implication graph to folder " << folder_name << "...");
  do {
    // ask the user to provide a folder name
    if (!actual_folder_name.empty()) {
      // check if the folder can be created
      try {
        create_folder_if_not_exists(actual_folder_name);
        break;
      } catch (const std::exception& e) {
        LOG_ERROR("Error: Could not create folder " << actual_folder_name << ". " << e.what());
        actual_folder_name = "";
        continue;
      }
    }
    cout << "Please provide a folder name to export the implication graph to: ";
    getline(cin, actual_folder_name);
    if (actual_folder_name.empty()) {
      cout << "Folder name cannot be empty. Please try again." << endl;
      continue;
    }
  } while (true);

  indexed_vector<vector<Tlit>, Tvar> implying_literals(nvars + 1);

  for (size_t i = 0; i < assignment.size(); i++) {
    Tlit lit = assignment[i];

    const Tlit* reason = get_reason(lit);
    if (reason == nullptr)
      continue;

    size_t reason_size = get_reason_size(lit);
    for (size_t j = 1; j < reason_size; j++) {
      Tlit implied_lit = reason[j];
      implying_literals[implied_lit.var()].push_back(lit);
    }
  }

  for (size_t i = 0; i < assignment.size(); i++) {
    Tlit lit = assignment[i];
    string content = lit_info(lit);
    create_file_for_literal(actual_folder_name, lit, content, implying_literals[lit.var()]);
  }
}

void implication_graph_exporter::create_folder_if_not_exists(const std::string& folder_name)
{
  LOG_INFO("Creating folder " << folder_name << "...");
  bool found_obsidian = false;
  if (!std::filesystem::exists(folder_name)) {
    std::filesystem::create_directory(folder_name);
  } else {
    for (const auto& entry : std::filesystem::directory_iterator(folder_name)) {
      // ignore the .obsidian folder if it exists
      if (entry.path().filename() == ".obsidian") {
        found_obsidian = true;
        continue;
      }
      std::filesystem::remove(entry.path());
    }
  }
  if (!found_obsidian) {
    // copy the .obsidian folder from obsidian_template if it exists
    std::string location = env::get_obsidian_template_folder();
    std::filesystem::path template_path = location + "/.obsidian";
    if (std::filesystem::exists(template_path)) {
      LOG_INFO("Copying obsidian template from " << template_path << " to " << folder_name + "/.obsidian");
      std::filesystem::copy(template_path, folder_name + "/.obsidian", std::filesystem::copy_options::recursive);
    } else {
      LOG_ERROR("Could not find the obsidian template folder at " << location);
    }
  }
}

const string VAULT_PLACEHOLDER = "VAULT_NAME";

void implication_graph_exporter::create_file_for_literal(const std::string& folder_name,
                                                        Tlit lit,
                                                        const std::string& content,
                                                        const std::vector<Tlit>& implying_literals)
{
  std::string file_name = folder_name + "/" + lit.to_string() + ".md";
  std::ofstream file(file_name);
  if (!file.is_open()) {
    LOG_ERROR("Error: Could not create file " << file_name);
    return;
  }

  // replace the placeholder VAULT_PLACEHOLDER in the content with the actual folder name
  std::string content_with_links = content;
  std::string vault_name = folder_name.substr(folder_name.find_last_of("/\\") + 1);

  size_t pos = content_with_links.find(VAULT_PLACEHOLDER);
  while (pos != std::string::npos) {
    content_with_links.replace(pos, VAULT_PLACEHOLDER.length(), vault_name);
    pos = content_with_links.find(VAULT_PLACEHOLDER, pos + vault_name.length());
  }

  file << content_with_links << endl;

  if (!implying_literals.empty()) {
    file << "\nImplying the following literals : \n";
    for (Tlit implied_lit : implying_literals) {
      file << "- [[" << implied_lit.to_string() << "]]\n";
    }
  }


  file.close();
}

}
