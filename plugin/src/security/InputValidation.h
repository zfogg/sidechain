#pragma once

#include <JuceHeader.h>
#include <functional>
#include <optional>
#include <regex>
#include <string>

namespace Sidechain {
namespace Security {

/**
 * InputValidation - Comprehensive input validation framework
 *
 * Validates and sanitizes user input at system boundaries:
 * - API request bodies
 * - File uploads
 * - User-generated content
 * - Configuration values
 *
 * Usage:
 *   auto validator = InputValidator::create()
 *       ->addRule("email", InputValidator::email())
 *       ->addRule("username",
 * InputValidator::alphanumeric().minLength(3).maxLength(20))
 *       ->addRule("age", InputValidator::integer().min(0).max(150));
 *
 *   auto result = validator->validate({
 *       {"email", "user@example.com"},
 *       {"username", "john_doe"},
 *       {"age", "25"}
 *   });
 *
 *   if (result.isValid())
 *       processInput(result.getValues());
 *   else
 *       showErrors(result.getErrors());
 */

class ValidationRule {
public:
  virtual ~ValidationRule() = default;

  /**
   * Validate a single value
   *
   * @param value The value to validate
   * @return Empty string if valid, error message if invalid
   */
  virtual juce::String validate(const juce::String &value) const = 0;

  /**
   * Get a human-readable description of this rule
   */
  virtual juce::String getDescription() const = 0;
};

/**
 * Result of validation operation
 */
class ValidationResult {
public:
  ValidationResult() : isValid_(true) {}

  explicit ValidationResult(const juce::StringPairArray &errors) : isValid_(errors.size() == 0), errors_(errors) {}

  /**
   * Check if validation passed
   */
  bool isValid() const {
    return isValid_;
  }

  /**
   * Get all validation errors
   */
  const juce::StringPairArray &getErrors() const {
    return errors_;
  }

  /**
   * Get error for specific field
   */
  std::optional<juce::String> getError(const juce::String &field) const {
    auto value = errors_[field];
    if (value.isNotEmpty())
      return value;
    return std::nullopt;
  }

  /**
   * Get validated and sanitized values
   */
  const juce::StringPairArray &getValues() const {
    return values_;
  }

  /**
   * Get sanitized value for field
   */
  std::optional<juce::String> getValue(const juce::String &field) const {
    auto value = values_[field];
    if (value.isNotEmpty())
      return value;
    return std::nullopt;
  }

  /**
   * Add or update an error
   */
  void addError(const juce::String &field, const juce::String &error) {
    errors_.set(field, error);
    isValid_ = false;
  }

  /**
   * Add or update a sanitized value
   */
  void setValue(const juce::String &field, const juce::String &value) {
    values_.set(field, value);
  }

private:
  bool isValid_;
  juce::StringPairArray errors_;
  juce::StringPairArray values_;
};

/**
 * StringRule - Base class for string validation rules
 */
class StringRule : public ValidationRule {
public:
  StringRule() : minLen_(0), maxLen_(std::numeric_limits<int>::max()), pattern_("") {}
  ~StringRule() override = default;

  StringRule &minLength(int len) {
    minLen_ = len;
    return *this;
  }

  StringRule &maxLength(int len) {
    maxLen_ = len;
    return *this;
  }

  StringRule &pattern(const juce::String &regex) {
    pattern_ = regex;
    return *this;
  }

  StringRule &custom(std::function<bool(const juce::String &)> fn) {
    customValidator_ = fn;
    return *this;
  }

  juce::String validate(const juce::String &value) const override {
    // Check length
    if (value.length() < minLen_)
      return juce::String("Minimum length is ") + juce::String(minLen_);

    if (value.length() > maxLen_)
      return juce::String("Maximum length is ") + juce::String(maxLen_);

    // Check pattern
    if (pattern_.isNotEmpty()) {
      try {
        std::regex re(pattern_.toStdString());
        if (!std::regex_match(value.toStdString(), re))
          return "Value does not match required pattern";
      } catch (const std::regex_error &) {
        return "Invalid pattern configuration";
      }
    }

    // Check custom validator
    if (customValidator_ && !customValidator_(value))
      return "Custom validation failed";

    return "";
  }

  juce::String getDescription() const override {
    return "String";
  }

protected:
  int minLen_;
  int maxLen_;
  juce::String pattern_;
  std::function<bool(const juce::String &)> customValidator_;
};

/**
 * IntegerRule - Validation for integer values
 */
class IntegerRule : public ValidationRule {
public:
  IntegerRule() : min_(std::numeric_limits<int>::min()), max_(std::numeric_limits<int>::max()) {}

  IntegerRule &min(int value) {
    min_ = value;
    return *this;
  }

  IntegerRule &max(int value) {
    max_ = value;
    return *this;
  }

  juce::String validate(const juce::String &value) const override {
    if (!value.containsOnly("-0123456789"))
      return "Must be a valid integer";

    int intValue = value.getIntValue();

    if (intValue < min_)
      return juce::String("Minimum value is ") + juce::String(min_);

    if (intValue > max_)
      return juce::String("Maximum value is ") + juce::String(max_);

    return "";
  }

  juce::String getDescription() const override {
    return "Integer";
  }

private:
  int min_;
  int max_;
};

/**
 * InputValidator - Main validation coordinator
 */
class InputValidator : public std::enable_shared_from_this<InputValidator> {
public:
  // Token to enable make_shared with public constructor
  struct PrivateToken {};

  /**
   * Create a new validator
   */
  static std::shared_ptr<InputValidator> create() {
    return std::make_shared<InputValidator>(PrivateToken{});
  }

  /**
   * Public constructor for make_shared (use create() instead)
   */
  explicit InputValidator(PrivateToken) {}

  /**
   * Email validation rule
   */
  static std::shared_ptr<StringRule> email() {
    auto rule = std::make_shared<StringRule>();
    rule->pattern("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return rule;
  }

  /**
   * Alphanumeric rule
   */
  static std::shared_ptr<StringRule> alphanumeric() {
    auto rule = std::make_shared<StringRule>();
    rule->pattern("^[a-zA-Z0-9_]+$");
    return rule;
  }

  /**
   * URL validation rule
   */
  static std::shared_ptr<StringRule> url() {
    auto rule = std::make_shared<StringRule>();
    rule->pattern("^https?://[a-zA-Z0-9.-]+(\\.[a-zA-Z]{2,})?.*$");
    return rule;
  }

  /**
   * Integer validation rule
   */
  static std::shared_ptr<IntegerRule> integer() {
    return std::make_shared<IntegerRule>();
  }

  /**
   * Generic string rule
   */
  static std::shared_ptr<StringRule> string() {
    return std::make_shared<StringRule>();
  }

  /**
   * Add validation rule for a field
   */
  template <typename RuleType>
  std::shared_ptr<InputValidator> addRule(const juce::String &field, std::shared_ptr<RuleType> rule) {
    static_assert(std::is_base_of<ValidationRule, RuleType>::value, "RuleType must derive from ValidationRule");
    rules_[field] = std::static_pointer_cast<ValidationRule>(rule);
    return shared_from_this();
  }

  /**
   * Validate a set of fields
   */
  ValidationResult validate(const juce::StringPairArray &input) {
    ValidationResult result;

    for (int i = 0; i < input.size(); ++i) {
      auto field = input.getAllKeys()[i];
      auto value = input.getAllValues()[i];

      // Check if field has a validation rule
      auto it = rules_.find(field);
      if (it != rules_.end()) {
        auto error = it->second->validate(value);
        if (error.isNotEmpty()) {
          result.addError(field, error);
          continue;
        }
      }

      // Sanitize and store value
      auto sanitized = sanitize(value);
      result.setValue(field, sanitized);
    }

    return result;
  }

  /**
   * Sanitize user input to prevent injection attacks
   */
  static juce::String sanitize(const juce::String &input) {
    auto sanitized = input;

    // Remove null bytes
    sanitized = sanitized.removeCharacters("\0");

    // Escape XML/HTML special characters
    sanitized = sanitized.replace("&", "&amp;");
    sanitized = sanitized.replace("<", "&lt;");
    sanitized = sanitized.replace(">", "&gt;");
    sanitized = sanitized.replace("\"", "&quot;");
    sanitized = sanitized.replace("'", "&#39;");

    // Remove dangerous control characters
    sanitized = sanitized.removeCharacters("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0e\x0f");

    return sanitized;
  }

  /**
   * Validate file upload
   */
  static ValidationResult validateFileUpload(const juce::File &file, size_t maxSizeBytes,
                                             const juce::StringArray &allowedExtensions) {
    ValidationResult result;

    if (!file.exists()) {
      result.addError("file", "File does not exist");
      return result;
    }

    // Check file size
    if (file.getSize() > static_cast<juce::int64>(maxSizeBytes)) {
      result.addError("file", "File is too large (max " + juce::String(maxSizeBytes / 1024 / 1024) + "MB)");
      return result;
    }

    // Check file extension
    auto extension = file.getFileExtension().toLowerCase();
    bool extensionAllowed = false;
    for (const auto &allowed : allowedExtensions) {
      if (extension == "." + allowed.toLowerCase()) {
        extensionAllowed = true;
        break;
      }
    }

    if (!extensionAllowed) {
      result.addError("file", "File type not allowed");
      return result;
    }

    result.setValue("file", file.getFullPathName());
    return result;
  }

private:
  std::map<juce::String, std::shared_ptr<ValidationRule>> rules_;
};

} // namespace Security
} // namespace Sidechain
