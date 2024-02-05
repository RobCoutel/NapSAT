#include "SAT-display.hpp"
#include "../environment.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

#include <execinfo.h>
#include <unistd.h>

void sat::gui::display::set_display_level(unsigned level)
{
  _display_level = level;
}

void sat::gui::display::back()
{
  while (!_observer->is_back_to_origin()) {
    unsigned level = _observer->back();
    if (level <= _display_level)
      break;
  }
  _updated = true;
}

bool sat::gui::display::next()
{
  while (!_observer->is_real_time()) {
    unsigned level = _observer->next();
    if (level <= _display_level)
      break;
  }
  _updated = true;
  return _observer->is_real_time();
}

void sat::gui::display::notify_change(unsigned level)
{
  if (level > _display_level)
    return;

  while (true) {
    if (_updated) {
      print_state();
      std::cout << "Display level: " << _display_level << std::endl;
      std::cout << "Notification number: " << _observer->notification_number() << std::endl;
      std::cout << "Last notification message: " << _observer->last_message() << std::endl;
    }
  loop_start:
    std::cout << "Navigation command: ";
    std::string command = "";
    std::getline(std::cin, command, '\n');
    // std::cout << command << std::endl;
    if (command == "next" || command == "") {
      next();
      if (_observer->is_real_time()) {
        std::cout << "Back to real time" << std::endl;
        return;
      }
    }
    else if (command == "back" || command == "b") {
      back();
    }
    else if (command == "print" || command == "p") {
      print_state();
    }
    else if (command == "quit" || command == "q") {
      // TODO exit gracefully
      exit(0);
    }
    else if (command.rfind("set level", 0) == 0) {
      if (command.size() < 10) {
        std::cout << "Invalid level (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        _display_level = std::stoi(command.substr(10));
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid level (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("mark var", 0) == 0) {
      if (command.size() < 10) {
        std::cout << "Invalid variable (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        Tlit lit = std::stoi(command.substr(9));
        _observer->mark_variable(lit);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid literal (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("unmark var", 0) == 0) {
      if (command.size() < 12) {
        std::cout << "Invalid variable (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        Tlit lit = std::stoi(command.substr(11));
        _observer->unmark_variable(lit);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid literal (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("mark clause", 0) == 0) {
      if (command.size() < 12) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        Tclause cl = std::stoi(command.substr(12));
        _observer->mark_clause(cl);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("unmark clause", 0) == 0) {
      if (command.size() < 14) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        Tclause cl = std::stoi(command.substr(13));
        _observer->unmark_clause(cl);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("set breakpoint", 0) == 0) {
      std::string pattern = "set breakpoint";
      unsigned command_len = pattern.size() + 1;
      if (command.size() < command_len) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        unsigned level = std::stoi(command.substr(command_len));
        _observer->set_breakpoint(level);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("remove breakpoint", 0) == 0) {
      if (command.size() < 17) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        unsigned level = std::stoi(command.substr(16));
        _observer->unset_breakpoint(level);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("print trail latex", 0) == 0) {
      std::string pattern = "print trail latex";
      unsigned command_len = pattern.size() + 1;
      std::string latex = _observer->trail_to_latex();
      if (command.size() > command_len) {
        std::string filename = command.substr(command_len);
        std::ofstream file(filename);
        if (!file.is_open()) {
          std::cout << "Could not open file " << filename << std::endl;
          goto loop_start;
        }
        file << latex;
        file.close();
      }
      else
        std::cout << latex << std::endl;
    }
    else if (command.rfind("mark clause", 0) == 0) {
      if (command.size() < 12) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        Tclause cl = std::stoi(command.substr(12));
        _observer->mark_clause(cl);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid clause (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("set breakpoint", 0) == 0) {
      std::string pattern = "set breakpoint";
      unsigned command_len = pattern.size() + 1;
      if (command.size() < command_len) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
      try {
        unsigned level = std::stoi(command.substr(command_len));
        _observer->set_breakpoint(level);
      }
      catch (std::invalid_argument const&) {
        std::cout << "Invalid breakpoint (positive integer expected)" << std::endl;
        goto loop_start;
      }
    }
    else if (command.rfind("print trail latex", 0) == 0) {
      std::string pattern = "print trail latex";
      unsigned command_len = pattern.size() + 1;
      std::string latex = _observer->trail_to_latex();
      if (command.size() > command_len) {
        std::string filename = command.substr(command_len);
        std::ofstream file(filename);
        if (!file.is_open()) {
          std::cout << "Could not open file " << filename << std::endl;
          goto loop_start;
        }
        file << latex;
        file.close();
      }
      else
        std::cout << latex << std::endl;
    }
    else if (command.rfind("print clauses latex", 0) == 0) {
      std::string pattern = "print clauses latex";
      unsigned command_len = pattern.size() + 1;
      std::string latex = _observer->clause_set_to_latex();
      if (command.size() > command_len) {
        std::string filename = command.substr(command_len);
        std::ofstream file(filename);
        if (!file.is_open()) {
          std::cout << "Could not open file " << filename << std::endl;
          goto loop_start;
        }
        file << latex;
        file.close();
      }
      else
        std::cout << latex << std::endl;
    }
    else if (command.rfind("print implications latex", 0) == 0) {
      std::string pattern = "print implications latex";
      unsigned command_len = pattern.size() + 1;
      std::string latex = _observer->implication_graph_to_latex();
      if (command.size() > command_len) {
        std::string filename = command.substr(command_len);
        std::ofstream file(filename);
        if (!file.is_open()) {
          std::cout << "Could not open file " << filename << std::endl;
          goto loop_start;
        }
        file << latex;
        file.close();
      }
      else
        std::cout << latex << std::endl;
    }
    else if (command == "start recording") {
      _observer->_recording = true;
    }
    else if (command == "stop recording") {
      _observer->_recording = false;
    }
    else if (command == "save") {
      bool old_record = _observer->_recording;
      _observer->_recording = true;
      _observer->save_state();
      _observer->_recording = old_record;
    }
    else if (command == "sort clauses") {
      observer::enable_sorting = true;
      _updated = true;
    }
    else if (command == "print trace") {
      const int maxStackTraceSize = 32;
      void* stackTrace[maxStackTraceSize];
      int stackSize = backtrace(stackTrace, maxStackTraceSize);
      char** stackSymbols = backtrace_symbols(stackTrace, stackSize);
      if (stackSymbols) {
        for (int i = 0; i < stackSize; ++i) {
          std::cout << stackSymbols[i] << std::endl;
        }
        free(stackSymbols);
      }
    }
    else if (command == "help" || command == "h") {
      // read the man page on the navigation
      std::string man_nav = sat::env::man_page_directory + "man-nav.txt";
      std::ifstream file(man_nav);
      if (file.is_open()) {
        std::string line;
        while (getline(file, line))
          std::cout << line << std::endl;
        file.close();
      }
      else {
        std::cerr << "Error: could not load the manual page." << std::endl;
      }
    }
    else {
      std::cout << "Unknown command" << std::endl;
      goto loop_start;
    }
  }
}

void sat::gui::display::notify_checkpoint()
{
  print_state();
  while (true) {
    std::string command;
    std::cout << "SAT command: ";
    std::getline(std::cin, command);
    if (_observer->transmit_command(command))
      break;
  }
}

void sat::gui::display::print_state()
{
  _observer->print_variables();
  _observer->print_clause_set();
  _observer->print_assignment();
  _updated = false;
}
