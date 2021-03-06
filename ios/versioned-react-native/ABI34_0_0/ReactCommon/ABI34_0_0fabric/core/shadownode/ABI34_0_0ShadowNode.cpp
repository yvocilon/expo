/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ABI34_0_0ShadowNode.h"

#include <string>

#include <ReactABI34_0_0/core/ShadowNodeFragment.h>
#include <ReactABI34_0_0/debug/DebugStringConvertible.h>
#include <ReactABI34_0_0/debug/debugStringConvertibleUtils.h>

namespace facebook {
namespace ReactABI34_0_0 {

SharedShadowNodeSharedList ShadowNode::emptySharedShadowNodeSharedList() {
  static const auto emptySharedShadowNodeSharedList =
      std::make_shared<SharedShadowNodeList>();
  return emptySharedShadowNodeSharedList;
}

#pragma mark - Constructors

ShadowNode::ShadowNode(
    const ShadowNodeFragment &fragment,
    const ShadowNodeCloneFunction &cloneFunction)
    : tag_(fragment.tag),
      rootTag_(fragment.rootTag),
      props_(fragment.props),
      eventEmitter_(fragment.eventEmitter),
      children_(fragment.children ?: emptySharedShadowNodeSharedList()),
      cloneFunction_(cloneFunction),
      childrenAreShared_(true),
      revision_(1) {
  assert(props_);
  assert(children_);
}

ShadowNode::ShadowNode(
    const ShadowNode &sourceShadowNode,
    const ShadowNodeFragment &fragment)
    : tag_(fragment.tag ?: sourceShadowNode.tag_),
      rootTag_(fragment.rootTag ?: sourceShadowNode.rootTag_),
      props_(fragment.props ?: sourceShadowNode.props_),
      eventEmitter_(fragment.eventEmitter ?: sourceShadowNode.eventEmitter_),
      children_(fragment.children ?: sourceShadowNode.children_),
      localData_(fragment.localData ?: sourceShadowNode.localData_),
      cloneFunction_(sourceShadowNode.cloneFunction_),
      childrenAreShared_(true),
      revision_(sourceShadowNode.revision_ + 1) {
  assert(props_);
  assert(children_);
}

UnsharedShadowNode ShadowNode::clone(const ShadowNodeFragment &fragment) const {
  assert(cloneFunction_);
  return cloneFunction_(*this, fragment);
}

#pragma mark - Getters

const SharedShadowNodeList &ShadowNode::getChildren() const {
  return *children_;
}

SharedProps ShadowNode::getProps() const {
  return props_;
}

SharedEventEmitter ShadowNode::getEventEmitter() const {
  return eventEmitter_;
}

Tag ShadowNode::getTag() const {
  return tag_;
}

Tag ShadowNode::getRootTag() const {
  return rootTag_;
}

SharedLocalData ShadowNode::getLocalData() const {
  return localData_;
}

void ShadowNode::sealRecursive() const {
  if (getSealed()) {
    return;
  }

  seal();

  props_->seal();

  for (auto child : *children_) {
    child->sealRecursive();
  }
}

#pragma mark - Mutating Methods

void ShadowNode::appendChild(const SharedShadowNode &child) {
  ensureUnsealed();

  cloneChildrenIfShared();
  auto nonConstChildren =
      std::const_pointer_cast<SharedShadowNodeList>(children_);
  nonConstChildren->push_back(child);
}

void ShadowNode::replaceChild(
    const SharedShadowNode &oldChild,
    const SharedShadowNode &newChild,
    int suggestedIndex) {
  ensureUnsealed();

  cloneChildrenIfShared();

  auto nonConstChildren =
      std::const_pointer_cast<SharedShadowNodeList>(children_);

  if (suggestedIndex != -1 && suggestedIndex < nonConstChildren->size()) {
    if (nonConstChildren->at(suggestedIndex) == oldChild) {
      (*nonConstChildren)[suggestedIndex] = newChild;
      return;
    }
  }

  std::replace(
      nonConstChildren->begin(), nonConstChildren->end(), oldChild, newChild);
}

void ShadowNode::setLocalData(const SharedLocalData &localData) {
  ensureUnsealed();
  localData_ = localData;
}

void ShadowNode::cloneChildrenIfShared() {
  if (!childrenAreShared_) {
    return;
  }
  childrenAreShared_ = false;
  children_ = std::make_shared<SharedShadowNodeList>(*children_);
}

void ShadowNode::setMounted(bool mounted) const {
  eventEmitter_->setEnabled(mounted);
}

bool ShadowNode::constructAncestorPath(
    const ShadowNode &ancestorShadowNode,
    std::vector<std::reference_wrapper<const ShadowNode>> &ancestors) const {
  // Note: We have a decent idea of how to make it reasonable performant.
  // This is not implemented yet though. See T36620537 for more details.
  if (this == &ancestorShadowNode) {
    return true;
  }

  for (const auto &childShadowNode : *ancestorShadowNode.children_) {
    if (constructAncestorPath(*childShadowNode, ancestors)) {
      ancestors.push_back(std::ref(ancestorShadowNode));
      return true;
    }
  }

  return false;
}

#pragma mark - DebugStringConvertible

#if RN_DEBUG_STRING_CONVERTIBLE
std::string ShadowNode::getDebugName() const {
  return getComponentName();
}

std::string ShadowNode::getDebugValue() const {
  return "r" + folly::to<std::string>(revision_) +
      (getSealed() ? "/sealed" : "");
}

SharedDebugStringConvertibleList ShadowNode::getDebugChildren() const {
  auto debugChildren = SharedDebugStringConvertibleList{};

  for (auto child : *children_) {
    auto debugChild =
        std::dynamic_pointer_cast<const DebugStringConvertible>(child);
    if (debugChild) {
      debugChildren.push_back(debugChild);
    }
  }

  return debugChildren;
}

SharedDebugStringConvertibleList ShadowNode::getDebugProps() const {
  return props_->getDebugProps() +
      SharedDebugStringConvertibleList{
          debugStringConvertibleItem("tag", folly::to<std::string>(tag_))};
}
#endif

} // namespace ReactABI34_0_0
} // namespace facebook
