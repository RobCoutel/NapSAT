/**
 * @file src/utils/options.hpp
 * @author Robin Coutelier
 *
 * @brief Generic, self-documenting command-line option system. An Option binds by reference to an
 * existing field (bool/double/string), knows its CLI aliases, and can declare fixed-priority
 * incompatibilities with other options (subsumes) and simple AND-requirements on other options
 * (require). OptionParser collects a set of Options, parses a token stream into them, then resolves
 * incompatibilities/requirements, and can print a help text describing every registered option.
 */
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace napsat
{
  class Option {
  public:
    Option(std::string name, std::string brief, std::string category);
    virtual ~Option() = default;

    const std::string& get_name() const { return _name; }
    const std::string& get_brief() const { return _brief; }
    const std::string& get_category() const { return _category; }
    const std::vector<std::string>& get_aliases() const { return _aliases; }
    bool was_set() const { return _was_set; }

    /** @brief Registers an additional CLI alias for this option. Chainable. */
    Option& alias(const std::string& a);
    /** @brief Declares that, when both options end up active, `*this` wins and `loser` is reset. */
    Option& subsumes(Option& loser);
    /** @brief Declares that this option is only valid when `dep`'s value equals `expected`. */
    Option& require(Option& dep, bool expected);

    bool matches(const std::string& token) const;

    /**
     * @brief Parses the value of this option starting at tokens[i]. May consume the following
     * token (advancing i) if the option takes a value. Returns false (and logs a warning) if the
     * value could not be parsed; the storage is left untouched in that case.
     */
    virtual bool consume(const std::vector<std::string>& tokens, unsigned& i) = 0;

    /** @brief Resets the bound storage to the value it had when the option was registered. */
    virtual void reset_to_default() = 0;
    /** @brief Whether the option is currently in a state that should trigger subsumption/requirement checks. */
    virtual bool truthy() const = 0;

    virtual std::string type_name() const = 0;
    virtual std::string default_to_string() const = 0;
    virtual std::string current_to_string() const = 0;

    /** @brief One formatted help block for this option (aliases, type/default, brief, requires, subsumed-by). */
    std::string help_entry() const;

    const std::vector<Option*>& get_subsumed() const { return _subsumed; }
    const std::vector<std::pair<Option*, bool>>& get_requirements() const { return _requirements; }

    void mark_set() { _was_set = true; }

  protected:
    std::string _name;
    std::string _brief;
    std::string _category;
    std::vector<std::string> _aliases;
    bool _was_set = false;
    std::vector<Option*> _subsumed;
    std::vector<Option*> _subsumed_by;
    std::vector<std::pair<Option*, bool>> _requirements;
  };

  class BoolOption : public Option {
  public:
    BoolOption(std::string name, bool& storage, std::string brief, std::string category);
    bool consume(const std::vector<std::string>& tokens, unsigned& i) override;
    void reset_to_default() override { _storage = _default; }
    bool truthy() const override { return _storage; }
    std::string type_name() const override { return "bool"; }
    std::string default_to_string() const override { return _default ? "on" : "off"; }
    std::string current_to_string() const override { return _storage ? "on" : "off"; }
    bool value() const { return _storage; }

  private:
    bool& _storage;
    bool _default;
  };

  class DoubleOption : public Option {
  public:
    DoubleOption(std::string name, double& storage, std::string brief, std::string category);
    /** @brief Restricts accepted values to [min, max]. If fatal, an out-of-range value aborts the program. */
    DoubleOption& range(double min, double max, bool fatal);
    bool consume(const std::vector<std::string>& tokens, unsigned& i) override;
    void reset_to_default() override { _storage = _default; }
    bool truthy() const override { return _was_set; }
    std::string type_name() const override { return "double"; }
    std::string default_to_string() const override { return std::to_string(_default); }
    std::string current_to_string() const override { return std::to_string(_storage); }
    double value() const { return _storage; }
    /** @brief Checked once by OptionParser::resolve() after parsing/requirement resolution. */
    void validate_range() const;

  private:
    double& _storage;
    double _default;
    bool _has_range = false;
    double _min = 0, _max = 0;
    bool _fatal = false;
  };

  class StringOption : public Option {
  public:
    StringOption(std::string name, std::string& storage, std::string brief, std::string category);
    bool consume(const std::vector<std::string>& tokens, unsigned& i) override;
    void reset_to_default() override { _storage = _default; }
    bool truthy() const override { return _was_set; }
    std::string type_name() const override { return "string"; }
    std::string default_to_string() const override { return "\"" + _default + "\""; }
    std::string current_to_string() const override { return "\"" + _storage + "\""; }
    const std::string& value() const { return _storage; }

  private:
    std::string& _storage;
    std::string _default;
  };

  class OptionParser {
  public:
    OptionParser() = default;
    OptionParser(const OptionParser&) = delete;
    OptionParser& operator=(const OptionParser&) = delete;

    /** @brief Tags every option added after this call (until the next call) with the given category. */
    void set_category(std::string name) { _current_category = std::move(name); }

    BoolOption& add_bool(const std::string& name, bool& storage, const std::string& brief);
    DoubleOption& add_double(const std::string& name, double& storage, const std::string& brief);
    StringOption& add_string(const std::string& name, std::string& storage, const std::string& brief);

    /** @brief Consumes tokens into the registered options. Unknown tokens and duplicate options are warned about. */
    void parse(std::vector<std::string>& tokens);

    /** @brief Runs subsumption and requirement rules to a fixpoint, then validates numeric ranges. */
    void resolve();

    /** @brief Full help text for every registered option, grouped by category. */
    std::string help_text(const std::string& header = "") const;

    const std::vector<Option*> get_options() const { return _index_order; }

  private:
    Option& register_option(std::unique_ptr<Option> option);

    std::vector<std::unique_ptr<Option>> _options;
    std::vector<Option*> _index_order;
    std::string _current_category;
  };

} // namespace napsat
