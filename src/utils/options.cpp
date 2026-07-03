/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/options.cpp
 * @author Robin Coutelier
 *
 * @brief Implementation of the generic Option/OptionParser system.
 */
#include "options.hpp"
#include "printer.hpp"

#include <cstdlib>
#include <sstream>

using namespace std;

namespace napsat
{
  /**************************************************************************************************/
  /*                                          Option                                                */
  /**************************************************************************************************/
  Option::Option(string name, string brief, string category) :
    _name(std::move(name)),
    _brief(std::move(brief)),
    _category(std::move(category))
  {
    _aliases.push_back(_name);
  }

  Option& Option::alias(const string& a)
  {
    _aliases.push_back(a);
    return *this;
  }

  Option& Option::subsumes(Option& loser)
  {
    _subsumed.push_back(&loser);
    loser._subsumed_by.push_back(this);
    return *this;
  }

  Option& Option::require(Option& dep, bool expected)
  {
    _requirements.emplace_back(&dep, expected);
    return *this;
  }

  bool Option::matches(const string& token) const
  {
    for (const string& a : _aliases)
      if (a == token)
        return true;
    return false;
  }

  string Option::help_entry() const
  {
    ostringstream out;
    out << _aliases[0];
    for (size_t i = 1; i < _aliases.size(); i++)
      out << ", " << _aliases[i];
    out << " (" << type_name() << ", default " << default_to_string() << ")\n";
    if (!_brief.empty())
      out << justify_string(_brief, 80, ' ', "    ") << "\n";
    if (!_requirements.empty()) {
      out << "    requires:\n      ";
      for (size_t i = 0; i < _requirements.size(); i++) {
        if (i)
          out << ",\n      ";
        out << _requirements[i].first->get_name() << "=" << (_requirements[i].second ? "on" : "off");
      }
      out << "\n";
    }
    if (!_subsumed.empty()) {
      out << "    subsumes:\n      ";
      for (size_t i = 0; i < _subsumed.size(); i++) {
        if (i)
          out << ",\n     ";
        out << _subsumed[i]->get_name();
      }
      out << "\n";
    }
    return out.str();
  }

  /**************************************************************************************************/
  /*                                        BoolOption                                              */
  /**************************************************************************************************/
  BoolOption::BoolOption(string name, bool& storage, string brief, string category) :
    Option(std::move(name), std::move(brief), std::move(category)),
    _storage(storage),
    _default(storage)
  {
  }

  bool BoolOption::consume(const vector<string>& tokens, unsigned& i)
  {
    string next_token = (i + 1 < tokens.size()) ? tokens[i + 1] : "";
    if (!next_token.empty() && next_token[0] != '-') {
      if (next_token == "on") {
        _storage = true;
      }
      else if (next_token == "off") {
        _storage = false;
      }
      else {
        LOG_WARNING("option " << tokens[i] << " requires a boolean value (on/off).");
        LOG_WARNING("Default value " << (_storage ? "on" : "off") << " is used.");
        return false;
      }
      i++;
    }
    else {
      _storage = true;
    }
    return true;
  }

  /**************************************************************************************************/
  /*                                       DoubleOption                                             */
  /**************************************************************************************************/
  DoubleOption::DoubleOption(string name, double& storage, string brief, string category) :
    Option(std::move(name), std::move(brief), std::move(category)),
    _storage(storage),
    _default(storage)
  {
  }

  DoubleOption& DoubleOption::range(double min, double max, bool fatal)
  {
    _has_range = true;
    _min = min;
    _max = max;
    _fatal = fatal;
    return *this;
  }

  bool DoubleOption::consume(const vector<string>& tokens, unsigned& i)
  {
    string next_token = (i + 1 < tokens.size()) ? tokens[i + 1] : "";
    if (next_token.empty() || next_token[0] == '-') {
      LOG_WARNING("option " << tokens[i] << " requires a value (floating point number).");
      LOG_WARNING("Default value " << _storage << " is used.");
      return false;
    }
    try {
      _storage = stod(next_token);
    }
    catch (const std::invalid_argument&) {
      LOG_WARNING("option " << tokens[i] << " requires a floating point number value.");
      LOG_WARNING("Default value " << _storage << " is used.");
      return false;
    }
    i++;
    return true;
  }

  void DoubleOption::validate_range() const
  {
    if (!_has_range)
      return;
    if (_storage < _min || _storage > _max) {
      if (_fatal) {
        LOG_ERROR(_name << " must be between " << _min << " and " << _max << ".");
        exit(1);
      }
      LOG_WARNING(_name << " must be between " << _min << " and " << _max << ".");
    }
  }

  /**************************************************************************************************/
  /*                                       StringOption                                             */
  /**************************************************************************************************/
  StringOption::StringOption(string name, string& storage, string brief, string category) :
    Option(std::move(name), std::move(brief), std::move(category)),
    _storage(storage),
    _default(storage)
  {
  }

  bool StringOption::consume(const vector<string>& tokens, unsigned& i)
  {
    string next_token = (i + 1 < tokens.size()) ? tokens[i + 1] : "";
    if (next_token.empty() || next_token[0] == '-') {
      LOG_WARNING("option " << tokens[i] << " requires a value (string of characters).");
      LOG_WARNING("The option is ignored.");
      return false;
    }
    _storage = next_token;
    i++;
    return true;
  }

  /**************************************************************************************************/
  /*                                       OptionParser                                             */
  /**************************************************************************************************/
  Option& OptionParser::register_option(unique_ptr<Option> option)
  {
    Option* raw = option.get();
    _options.push_back(std::move(option));
    _index_order.push_back(raw);
    return *raw;
  }

  BoolOption& OptionParser::add_bool(const string& name, bool& storage, const string& brief)
  {
    auto opt = make_unique<BoolOption>(name, storage, brief, _current_category);
    return static_cast<BoolOption&>(register_option(std::move(opt)));
  }

  DoubleOption& OptionParser::add_double(const string& name, double& storage, const string& brief)
  {
    auto opt = make_unique<DoubleOption>(name, storage, brief, _current_category);
    return static_cast<DoubleOption&>(register_option(std::move(opt)));
  }

  StringOption& OptionParser::add_string(const string& name, string& storage, const string& brief)
  {
    auto opt = make_unique<StringOption>(name, storage, brief, _current_category);
    return static_cast<StringOption&>(register_option(std::move(opt)));
  }

  void OptionParser::parse(vector<string>& tokens)
  {
    for (unsigned i = 0; i < tokens.size(); i++) {
      const string token = tokens[i];
      Option* match = nullptr;
      for (Option* opt : _index_order) {
        if (opt->matches(token)) {
          match = opt;
          break;
        }
      }
      if (!match) {
        LOG_WARNING("Unknown option " << token);
        continue;
      }
      if (match->was_set()) {
        LOG_WARNING("option " << token << " already set. The second occurrence is ignored.");
        continue;
      }
      if (match->consume(tokens, i))
        match->mark_set();
    }
  }

  void OptionParser::resolve()
  {
    bool changed = true;
    while (changed) {
      changed = false;
      for (Option* opt : _index_order) {
        if (!opt->truthy())
          continue;
        for (Option* loser : opt->get_subsumed()) {
          if (loser->truthy()) {
            LOG_WARNING(opt->get_name() << " subsumes " << loser->get_name() << ".");
            LOG_WARNING("The solver will run with " << opt->get_name() << ".");
            loser->reset_to_default();
            changed = true;
          }
        }
      }
      for (Option* opt : _index_order) {
        if (!opt->truthy())
          continue;
        bool ok = true;
        for (const auto& [dep, expected] : opt->get_requirements()) {
          if (dep->truthy() != expected) {
            ok = false;
            break;
          }
        }
        if (!ok) {
          LOG_WARNING(opt->get_name() << " requires an incompatible combination of options.");
          LOG_WARNING("The option is ignored.");
          opt->reset_to_default();
          changed = true;
        }
      }
    }
    for (Option* opt : _index_order) {
      if (auto* d = dynamic_cast<DoubleOption*>(opt))
        d->validate_range();
    }
  }

  string OptionParser::help_text(const string& header) const
  {
    ostringstream out;
    out << header;
    string last_category;
    bool first = true;
    for (Option* opt : _index_order) {
      if (first || opt->get_category() != last_category) {
        last_category = opt->get_category();
        first = false;
        if (!last_category.empty())
          out << "\n*** " << last_category << " ***\n";
      }
      out << "  " << opt->help_entry();
      out << "\n";
    }
    return out.str();
  }

} // namespace napsat
