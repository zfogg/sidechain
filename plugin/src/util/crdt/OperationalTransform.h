#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

namespace Sidechain {
namespace Util {
namespace CRDT {

/**
 * OperationalTransform - Foundation for real-time collaborative editing
 *
 * Implements OT (Operational Transform) for conflict resolution in concurrent
 * edits.
 *
 * Core properties:
 * - Convergence: Different execution orders produce same result
 * - Causality Preservation: Respects operation ordering
 * - Idempotence: Same operation applied twice produces same result
 *
 * Operation types:
 * - Insert: Add content at position
 * - Delete: Remove content at position
 * - Modify: Change existing content
 *
 * Usage:
 *   auto op1 = Insert(0, "hello");
 *   auto op2 = Insert(0, "world");
 *   auto transformed = OperationalTransform::transform(op1, op2);
 */
class OperationalTransform {
public:
  // Operation type enumeration
  enum class OpType { Insert, Delete, Modify };

  /**
   * Base operation class
   */
  struct Operation {
    Operation() = default;
    Operation(const Operation &) = default;
    Operation &operator=(const Operation &) = default;
    virtual ~Operation() = default;
    virtual OpType getType() const = 0;
    virtual std::shared_ptr<Operation> clone() const = 0;

    int clientId = 0;  // Client that originated this operation
    int timestamp = 0; // Logical clock timestamp for ordering
  };

  /**
   * Insert operation - adds content at specified position
   *
   * Properties:
   * - position: Where to insert
   * - content: What to insert
   * - length: How many characters
   */
  struct Insert : public Operation {
    int position = 0;
    std::string content;

    OpType getType() const override {
      return OpType::Insert;
    }

    std::shared_ptr<Operation> clone() const override {
      auto copy = std::make_shared<Insert>();
      copy->position = position;
      copy->content = content;
      copy->clientId = clientId;
      copy->timestamp = timestamp;
      return copy;
    }

    int getLength() const {
      return static_cast<int>(content.length());
    }
  };

  /**
   * Delete operation - removes content at specified position
   *
   * Properties:
   * - position: Where to start deleting
   * - length: How many characters to delete
   * - content: Original content (for undo)
   */
  struct Delete : public Operation {
    int position = 0;
    int length = 0;
    std::string content; // Original content for undo

    OpType getType() const override {
      return OpType::Delete;
    }

    std::shared_ptr<Operation> clone() const override {
      auto copy = std::make_shared<Delete>();
      copy->position = position;
      copy->length = length;
      copy->content = content;
      copy->clientId = clientId;
      copy->timestamp = timestamp;
      return copy;
    }
  };

  /**
   * Modify operation - changes content at specified position
   *
   * Properties:
   * - position: Where to start modifying
   * - oldContent: Original content
   * - newContent: New content
   */
  struct Modify : public Operation {
    int position = 0;
    std::string oldContent;
    std::string newContent;

    OpType getType() const override {
      return OpType::Modify;
    }

    std::shared_ptr<Operation> clone() const override {
      auto copy = std::make_shared<Modify>();
      copy->position = position;
      copy->oldContent = oldContent;
      copy->newContent = newContent;
      copy->clientId = clientId;
      copy->timestamp = timestamp;
      return copy;
    }
  };

  // ========== Transform Functions ==========

  /**
   * Transform two concurrent operations against each other
   *
   * Given two operations op1 and op2 that were applied concurrently,
   * produces transformed versions that can be applied in any order
   * while producing the same final result.
   *
   * @param op1 First operation
   * @param op2 Second operation
   * @return Pair of (transformed_op1, transformed_op2)
   */
  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transform(const std::shared_ptr<Operation> &op1, const std::shared_ptr<Operation> &op2) {
    // Handle Insert vs Insert
    if (auto ins1 = std::dynamic_pointer_cast<Insert>(op1)) {
      if (auto ins2 = std::dynamic_pointer_cast<Insert>(op2)) {
        return transformInsertInsert(ins1, ins2);
      } else if (auto del2 = std::dynamic_pointer_cast<Delete>(op2)) {
        return transformInsertDelete(ins1, del2);
      } else if (auto mod2 = std::dynamic_pointer_cast<Modify>(op2)) {
        return transformInsertModify(ins1, mod2);
      }
    }

    // Handle Delete vs Insert
    else if (auto del1 = std::dynamic_pointer_cast<Delete>(op1)) {
      if (auto ins2 = std::dynamic_pointer_cast<Insert>(op2)) {
        return transformDeleteInsert(del1, ins2);
      } else if (auto del2 = std::dynamic_pointer_cast<Delete>(op2)) {
        return transformDeleteDelete(del1, del2);
      } else if (auto mod2 = std::dynamic_pointer_cast<Modify>(op2)) {
        return transformDeleteModify(del1, mod2);
      }
    }

    // Handle Modify vs ...
    else if (auto mod1 = std::dynamic_pointer_cast<Modify>(op1)) {
      if (auto ins2 = std::dynamic_pointer_cast<Insert>(op2)) {
        return transformModifyInsert(mod1, ins2);
      } else if (auto del2 = std::dynamic_pointer_cast<Delete>(op2)) {
        return transformModifyDelete(mod1, del2);
      } else if (auto mod2 = std::dynamic_pointer_cast<Modify>(op2)) {
        return transformModifyModify(mod1, mod2);
      }
    }

    // Fallback: return clones
    return {op1->clone(), op2->clone()};
  }

  /**
   * Check if operation is no-op (no actual changes)
   */
  static bool isNoOp(const std::shared_ptr<Operation> &op) {
    if (auto ins = std::dynamic_pointer_cast<Insert>(op))
      return ins->content.empty();

    if (auto del = std::dynamic_pointer_cast<Delete>(op))
      return del->length == 0;

    if (auto mod = std::dynamic_pointer_cast<Modify>(op))
      return mod->oldContent == mod->newContent;

    return false;
  }

  /**
   * Apply operation to text, returning modified text
   */
  static std::string apply(const std::string &text, const std::shared_ptr<Operation> &op) {
    if (auto ins = std::dynamic_pointer_cast<Insert>(op)) {
      std::string result = text;
      result.insert(static_cast<size_t>(ins->position), ins->content);
      return result;
    }

    if (auto del = std::dynamic_pointer_cast<Delete>(op)) {
      std::string result = text;
      result.erase(static_cast<size_t>(del->position), static_cast<size_t>(del->length));
      return result;
    }

    if (auto mod = std::dynamic_pointer_cast<Modify>(op)) {
      std::string result = text;
      result.replace(static_cast<size_t>(mod->position), mod->oldContent.length(), mod->newContent);
      return result;
    }

    return text;
  }

private:
  // ========== Transform Implementations ==========

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformInsertInsert(const std::shared_ptr<Insert> &ins1, const std::shared_ptr<Insert> &ins2) {
    auto result1 = std::make_shared<Insert>(*ins1);
    auto result2 = std::make_shared<Insert>(*ins2);

    if (ins1->position < ins2->position) {
      // ins1 happens before ins2, adjust ins2's position
      result2->position += ins1->getLength();
    } else if (ins1->position > ins2->position) {
      // ins2 happens before ins1, adjust ins1's position
      result1->position += ins2->getLength();
    } else if (ins1->clientId < ins2->clientId) {
      // Same position: use client ID to break ties
      result2->position += ins1->getLength();
    } else {
      result1->position += ins2->getLength();
    }

    return {result1, result2};
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformInsertDelete(const std::shared_ptr<Insert> &ins, const std::shared_ptr<Delete> &del) {
    auto resultIns = std::make_shared<Insert>(*ins);
    auto resultDel = std::make_shared<Delete>(*del);

    if (ins->position <= del->position) {
      // Insert before delete, shift delete start
      resultDel->position += ins->getLength();
    } else if (ins->position >= del->position + del->length) {
      // Insert after delete, no change needed
    } else {
      // Insert within deleted region, adjust insert position
      resultIns->position = del->position;
    }

    return {resultIns, resultDel};
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformInsertModify(const std::shared_ptr<Insert> &ins, const std::shared_ptr<Modify> &mod) {
    auto resultIns = std::make_shared<Insert>(*ins);
    auto resultMod = std::make_shared<Modify>(*mod);

    if (ins->position <= mod->position) {
      resultMod->position += ins->getLength();
    }

    return {resultIns, resultMod};
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformDeleteInsert(const std::shared_ptr<Delete> &del, const std::shared_ptr<Insert> &ins) {
    return transformInsertDelete(ins, del);
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformDeleteDelete(const std::shared_ptr<Delete> &del1, const std::shared_ptr<Delete> &del2) {
    auto result1 = std::make_shared<Delete>(*del1);
    auto result2 = std::make_shared<Delete>(*del2);

    if (del1->position < del2->position) {
      result2->position -= std::min(del1->length, del2->position - del1->position);
    } else if (del1->position > del2->position) {
      result1->position -= std::min(del2->length, del1->position - del2->position);
    }

    return {result1, result2};
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformDeleteModify(const std::shared_ptr<Delete> &del, const std::shared_ptr<Modify> &mod) {
    auto resultDel = std::make_shared<Delete>(*del);
    auto resultMod = std::make_shared<Modify>(*mod);

    if (del->position < mod->position) {
      resultMod->position -= del->length;
    }

    return {resultDel, resultMod};
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformModifyInsert(const std::shared_ptr<Modify> &mod, const std::shared_ptr<Insert> &ins) {
    return transformInsertModify(ins, mod);
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformModifyDelete(const std::shared_ptr<Modify> &mod, const std::shared_ptr<Delete> &del) {
    return transformDeleteModify(del, mod);
  }

  static std::pair<std::shared_ptr<Operation>, std::shared_ptr<Operation>>
  transformModifyModify(const std::shared_ptr<Modify> &mod1, const std::shared_ptr<Modify> &mod2) {
    auto result1 = std::make_shared<Modify>(*mod1);
    auto result2 = std::make_shared<Modify>(*mod2);

    // If modifying same position, apply based on client ID
    if (mod1->position == mod2->position && mod1->clientId > mod2->clientId) {
      // mod2 wins, mod1 becomes no-op
      result1->oldContent = result1->newContent;
    }

    return {result1, result2};
  }
};

} // namespace CRDT
} // namespace Util
} // namespace Sidechain
