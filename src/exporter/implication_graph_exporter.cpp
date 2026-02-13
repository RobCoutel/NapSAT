#include "implication_graph_exporter.hpp"

#include "SAT-options.hpp"

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
  while (folder_name.empty()) {
    // ask the user to provide a folder name
    cout << "Please provide a folder name to export the implication graph to: ";
    string input;
    getline(cin, input);
    if (input.empty()) {
      cout << "Folder name cannot be empty. Please try again." << endl;
      continue;
    }
    // check if the folder can be created
    try {
      create_folder_if_not_exists(input);
    } catch (const std::exception& e) {
      cerr << "Error: Could not create folder " << input << ". " << e.what() << endl;
      continue;
    }
    return export_implication_graph(input, nvars, assignment, get_reason, get_reason_size, lit_info);
  }

  cout << "Exporting implication graph to folder " << folder_name << "..." << endl;
  create_folder_if_not_exists(folder_name);

  vector<vector<Tlit>> implying_literals(nvars + 1);

  for (size_t i = 0; i < assignment.size(); i++) {
    Tlit lit = assignment[i];

    const Tlit* reason = get_reason(lit);
    if (reason == nullptr)
      continue;

    size_t reason_size = get_reason_size(lit);
    for (size_t j = 1; j < reason_size; j++) {
      Tlit implied_lit = reason[j];
      implying_literals[lit_to_var(implied_lit)].push_back(lit);
    }
  }

  for (size_t i = 0; i < assignment.size(); i++) {
    Tlit lit = assignment[i];
    string content = lit_info(lit);
    create_file_for_literal(folder_name, lit, content, implying_literals[lit_to_var(lit)]);
  }
}

void implication_graph_exporter::create_folder_if_not_exists(const std::string& folder_name)
{
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
      std::filesystem::copy(template_path, folder_name + "/.obsidian", std::filesystem::copy_options::recursive);
    } else {
      cerr << "Warning: .obsidian template folder not found. The exported graph may not be properly recognized by Obsidian. Please create a folder named 'obsidian_template' with a subfolder named '.obsidian' containing the necessary Obsidian configuration files to fix this issue." << endl;
    }
  }
}

void implication_graph_exporter::create_file_for_literal(const std::string& folder_name,
                                                        Tlit lit,
                                                        const std::string& content,
                                                        const std::vector<Tlit>& implying_literals)
{
  std::string file_name = folder_name + "/" + to_string(lit_to_int(lit)) + ".md";
  std::ofstream file(file_name);
  if (!file.is_open()) {
    cerr << "Error: Could not create file " << file_name << endl;
    return;
  }

  file << content << endl;

  if (!implying_literals.empty()) {
    file << "\nImplying the following literals : \n";
    for (Tlit implied_lit : implying_literals) {
      file << "- [[" << lit_to_int(implied_lit) << "]]\n";
    }
  }


  file.close();
}

}
