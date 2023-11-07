#include "SAT-display.hpp"

#include <cassert>
#include <iostream>
#include <string>

void sat::gui::display::set_display_level(unsigned level)
{
  _display_level = level;
}

void sat::gui::display::back()
{
  while (!_observer->is_back_to_origin())
  {
    unsigned level = _observer->back();
    if (level <= _display_level)
      break;
  }
}

bool sat::gui::display::next()
{
  while (!_observer->is_real_time())
  {
    unsigned level = _observer->next();
    if (level <= _display_level)
      break;
  }
  return _observer->is_real_time();
}

void sat::gui::display::notify_change(unsigned level)
{
  if (level > _display_level)
    return;

  while (true)
  {
    print_state();
    std::cout << "Display level: " << _display_level << std::endl;
    loop_start:
    std::cout << "Navigation commands: ";
    std::string command;
    std::getline(std::cin, command, '\n');
    std::cout << command << std::endl;
    if (command == "next" || command == "")
    {
      next();
      if (_observer->is_real_time()) {
        std::cout << "Back to real time" << std::endl;
        return;
      }
    }
    else if (command == "back" || command == "b")
      back();
    else if (command == "quit" || command == "q")
      exit(0);
    else if (command.rfind("set level", 0) == 0)
      _display_level = std::stoi(command.substr(10));
    else if (command == "help" || command == "h")
    {
      std::cout << "next: go to next display level" << std::endl;
      std::cout << "back: go back to previous display level" << std::endl;
      std::cout << "set level <level>: set display level to <level>" << std::endl;
      std::cout << "quit: quit the program" << std::endl;
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
  while(true)
  {
    std::string command;
    std::cout << "Enter command: ";
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
}
