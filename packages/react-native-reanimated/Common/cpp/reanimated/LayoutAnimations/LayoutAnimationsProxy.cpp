#include <reanimated/LayoutAnimations/LayoutAnimationsProxy.h>
#include <reanimated/NativeModules/ReanimatedModuleProxy.h>
#ifndef ANDROID
#include <react/renderer/components/rnscreens/Props.h>
#endif
#include <react/renderer/animations/utils.h>
#include <react/renderer/mounting/ShadowViewMutation.h>
#include <reanimated/Tools/ReanimatedSystraceSection.h>
#include <glog/logging.h>
#include <react/renderer/components/scrollview/ScrollViewShadowNode.h>

#include <set>
#include <utility>

namespace reanimated {

// We never modify the Shadow Tree, we just send some additional
// mutations to the mounting layer.
// When animations finish, the Host Tree will represent the most recent Shadow
// Tree
// On android this code will be sometimes executed on the JS thread.
// That's why we have to schedule some of animation manager function on the UI
// thread
//std::optional<MountingTransaction> LayoutAnimationsProxy::pullTransaction(SurfaceId surfaceId, MountingTransaction::Number transactionNumber, const TransactionTelemetry &telemetry, ShadowViewMutationList mutations) const {
//#ifdef LAYOUT_ANIMATIONS_LOGS
//  LOG(INFO) << std::endl;
//  LOG(INFO) << "pullTransaction " << std::this_thread::get_id() << " "
//            << surfaceId << std::endl;
//#endif
//  LOG(INFO) << "pullTransaction";
//  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
//  ReanimatedSystraceSection d("pullTransaction");
//  PropsParserContext propsParserContext{surfaceId, *contextContainer_};
//  ShadowViewMutationList filteredMutations;
//  LightNode::Unshared beforeTopScreen, afterTopScreen;
//  std::unordered_map<SharedTag, std::pair<ShadowView, Tag>> afterMap;
//  std::unordered_map<SharedTag, std::pair<ShadowView, Tag>> beforeMap;
//  std::vector<std::shared_ptr<MutationNode>> roots;
//  std::unordered_map<Tag, ShadowView> movedViews;
//  bool isInTransition = transitionState_;
//
//  handleProgressTransition(filteredMutations, mutations, propsParserContext, surfaceId);
//      
//  if (!isInTransition){
//    
//    auto root = lightNodes_[surfaceId];
//    beforeTopScreen = topScreen[surfaceId];
//    
//    if (beforeTopScreen){
//      findSharedElementsOnScreen(beforeTopScreen, beforeMap);
//    }
//    
//    updateLightTree(mutations);
//    
//    root = lightNodes_[surfaceId];
//    afterTopScreen = findTopScreen(root);
//    
//    topScreen[surfaceId] = afterTopScreen;
//    if (afterTopScreen){
//      findSharedElementsOnScreen(afterTopScreen, afterMap);
//    }
//    
//    hideTransitioningViews(afterMap, afterTopScreen, beforeMap, beforeTopScreen, filteredMutations, propsParserContext);
//  } else {
//    updateLightTree(mutations);
//  }
//  
//  parseRemoveMutations(movedViews, mutations, roots);
//      
//  cleanupSharedTransitions(filteredMutations, propsParserContext, surfaceId);
//
//  handleRemovals(filteredMutations, roots);
//
//  handleUpdatesAndEnterings(
//      filteredMutations, movedViews, mutations, propsParserContext, surfaceId);
//
//  addOngoingAnimations(surfaceId, filteredMutations);
//      
//  if (!isInTransition){
//    handleSharedTransitionsStart(afterMap, afterTopScreen, beforeMap, beforeTopScreen, filteredMutations, mutations, propsParserContext, surfaceId);
//  }
//  
//  return MountingTransaction{
//      surfaceId, transactionNumber, std::move(filteredMutations), telemetry};
//}

std::optional<MountingTransaction> LayoutAnimationsProxy::pullTransaction(SurfaceId surfaceId, MountingTransaction::Number transactionNumber, const TransactionTelemetry &telemetry, ShadowViewMutationList mutations) const {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << std::endl;
  LOG(INFO) << "pullTransaction " << std::this_thread::get_id() << " "
            << surfaceId << std::endl;
#endif
  LOG(INFO) << "pullTransaction";
  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
  ReanimatedSystraceSection d("pullTransaction");
  PropsParserContext propsParserContext{surfaceId, *contextContainer_};
  ShadowViewMutationList filteredMutations;
  std::vector<std::shared_ptr<MutationNode>> roots;
  std::unordered_map<Tag, ShadowView> movedViews;
  bool isInTransition = transitionState_;
  
  if (isInTransition){
    updateLightTree(mutations);
    filteredMutations.insert(filteredMutations.end(), mutations.begin(), mutations.end());
    handleProgressTransition(filteredMutations, mutations, propsParserContext, surfaceId);
  } else if (!synchronized_){
    auto actualTop = topScreen[surfaceId];
    updateLightTree(mutations);
    auto reactTop = findTopScreen(lightNodes_[surfaceId]);
    if (reactTop->current.tag == actualTop->current.tag){
      synchronized_ = true;
    }
    filteredMutations.insert(filteredMutations.end(), mutations.begin(), mutations.end());
  } else {
    auto root = lightNodes_[surfaceId];
    auto beforeTopScreen = topScreen[surfaceId];
    if (beforeTopScreen){
      findSharedElementsOnScreen(beforeTopScreen, 0);
    }
    
    updateLightTree(mutations);
    
    root = lightNodes_[surfaceId];
    auto afterTopScreen = findTopScreen(root);
    
    topScreen[surfaceId] = afterTopScreen;
    if (afterTopScreen){
      findSharedElementsOnScreen(afterTopScreen, 1);
    }
    bool shouldTransitionStart = beforeTopScreen && afterTopScreen && beforeTopScreen->current.tag != afterTopScreen->current.tag;
    
    if (shouldTransitionStart){
      hideTransitioningViews(0, filteredMutations, propsParserContext);
      filteredMutations.insert(filteredMutations.end(), mutations.begin(), mutations.end());
      hideTransitioningViews(1, filteredMutations, propsParserContext);
    } else {
      filteredMutations.insert(filteredMutations.end(), mutations.begin(), mutations.end());
    }
    
    handleSharedTransitionsStart(afterTopScreen, beforeTopScreen, filteredMutations, mutations, propsParserContext, surfaceId);
  }
      
  cleanupSharedTransitions(filteredMutations, propsParserContext, surfaceId);

  addOngoingAnimations(surfaceId, filteredMutations);
  
  transitionMap_.clear();
  transitions_.clear();
  
  return MountingTransaction{
      surfaceId, transactionNumber, std::move(filteredMutations), telemetry};
}

Tag LayoutAnimationsProxy::findVisible(std::shared_ptr<LightNode> node, int& count) const{
//  auto group = sharedTransitionManager_->groups_[sharedTransitionManager_->tagToName_[node->current.tag]];
//  while (node != nullptr){
//    if (!strcmp(node->current.componentName, "RNSScreenStack")){
//
//    }
//    node = node->parent.lock();
//  }
  int c = count;
  if (!strcmp(node->current.componentName, "RNSScreen")){
    LOG(INFO) << c <<" begin screen tag: " << node->current.tag<<std::endl;
    count++;
  }
  for (auto& child: node->children){
    findVisible(child, count);
  }
  if (!strcmp(node->current.componentName, "RNSScreen")){
    LOG(INFO) << c <<" end screen" <<std::endl;
  }
  return -1;
}

LightNode::Unshared LayoutAnimationsProxy::findTopScreen(LightNode::Unshared node) const{
  LightNode::Unshared result = nullptr;
  if (!node->current.componentName){
    return result;
  }

  if (!(strcmp(node->current.componentName, "RNSScreen"))){
      bool isActive = false;
#ifdef ANDROID
      float f = node->current.props->rawProps.getDefault("activityState", 0).asDouble();
      isActive = f == 2.0f;
#else
      isActive = std::static_pointer_cast<const RNSScreenProps>(node->current.props)->activityState == 2.0f;
#endif
      if (isActive) {
          result = node;
      }
  }
  
  for (auto it = node->children.rbegin(); it != node->children.rend(); it++){
    auto t = findTopScreen(*it);
    if (t){
      return t;
    }
  }
  
  return result;
}

void LayoutAnimationsProxy::findSharedElementsOnScreen(LightNode::Unshared node, int index) const{
  if (sharedTransitionManager_->tagToName_.contains(node->current.tag)){
    ShadowView copy = node->current;
    copy.layoutMetrics = getAbsoluteMetrics(node);
    auto sharedTag = sharedTransitionManager_->tagToName_[node->current.tag];
    auto& transition = transitionMap_[sharedTag];
    transition.snapshot[index] = copy;
    transition.parentTag[index] = node->parent.lock()->current.tag;
    if (transition.parentTag[0] && transition.parentTag[1]){
      transitions_.push_back({sharedTag, transition});
    }
  }
  for (auto& child: node->children){
    findSharedElementsOnScreen(child, index);
  }
}

LayoutMetrics LayoutAnimationsProxy::getAbsoluteMetrics(LightNode::Unshared node) const{
  auto result = node->current.layoutMetrics;
  auto parent = node->parent.lock();
  while (parent){
    if (!strcmp(parent->current.componentName, "ScrollView")){
      auto state = std::static_pointer_cast<const ScrollViewShadowNode::ConcreteState>(parent->current.state);
      auto data = state->getData();
//      LOG(INFO) << node->current.tag << " content offset:" << data.contentOffset.x << " " << data.contentOffset.y;
      result.frame.origin -= data.contentOffset;
    }
    if (!strcmp(parent->current.componentName, "RNSScreen") && parent->children.size()>=2){
      auto p =parent->parent.lock();
      if (p){
        result.frame.origin.y += (p->current.layoutMetrics.frame.size.height - parent->current.layoutMetrics.frame.size.height);
      }
    }
    result.frame.origin.x += parent->current.layoutMetrics.frame.origin.x;
    result.frame.origin.y += parent->current.layoutMetrics.frame.origin.y;
    parent = parent->parent.lock();
  }
  return result;
}

void LayoutAnimationsProxy::handleProgressTransition(ShadowViewMutationList &filteredMutations, const ShadowViewMutationList &mutations, const PropsParserContext &propsParserContext, SurfaceId surfaceId) const {
  LOG(INFO) << "Transition state: " << transitionState_;
  if (!transitionUpdated_){
    return;
  }
  transitionUpdated_ = false;
  
  if (mutations.size() == 0 && transitionState_){
    if (transitionState_ == START){
      synchronized_ = false;
      auto root = lightNodes_[surfaceId];
      auto beforeTopScreen = topScreen[surfaceId];
      auto afterTopScreen = lightNodes_[transitionTag_];
      topScreen[surfaceId] = afterTopScreen;
      if (beforeTopScreen && afterTopScreen){
        LOG(INFO) << "start progress transition: " << beforeTopScreen->current.tag << " -> " << afterTopScreen->current.tag;
        
        findSharedElementsOnScreen(beforeTopScreen, 0);
        findSharedElementsOnScreen(afterTopScreen, 1);
        
        if (beforeTopScreen->current.tag != afterTopScreen->current.tag){
          
          for (auto& [sharedTag, transition]: transitions_){
            const auto& [before, after] = transition.snapshot;
            const auto& [beforeParentTag, afterParentTag] = transition.parentTag;
            
            auto& root = lightNodes_[surfaceId];
            ShadowView s = before;
            s.tag = myTag;
            filteredMutations.push_back(ShadowViewMutation::CreateMutation(s));
            filteredMutations.push_back(ShadowViewMutation::InsertMutation(surfaceId, s, root->children.size()));
            filteredMutations.push_back(ShadowViewMutation::UpdateMutation(after, after, afterParentTag));
            auto p = lightNodes_[before.tag]->parent.lock();
            auto m1 = ShadowViewMutation::InsertMutation(p->current.tag, before, 8);
            filteredMutations.push_back(ShadowViewMutation::UpdateMutation(before, *cloneViewWithoutOpacity(m1, propsParserContext), p->current.tag));
            
            
            auto m = ShadowViewMutation::UpdateMutation(after, after, afterParentTag);
            m = ShadowViewMutation::UpdateMutation(after, *cloneViewWithoutOpacity(m, propsParserContext), afterParentTag);
            filteredMutations.push_back(m);
            auto node = std::make_shared<LightNode>();
            node->current = s;
            lightNodes_[myTag] = node;
            
            root->children.push_back(node);
            layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[myTag] = layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[before.tag];
            ShadowView copy = after;
            copy.tag = myTag;
            auto copy2 = before;
            copy2.tag = myTag;
            startProgressTransition(myTag, copy2, copy, surfaceId);
            restoreMap_[myTag] = after.tag;
            sharedTransitionManager_->groups_[sharedTag].fakeTag = myTag;
            activeTransitions_.insert(myTag);
            myTag+=2;
            
          }
        }
      }
    } else if (transitionState_ == ACTIVE) {
      for (auto tag: activeTransitions_){
        auto layoutAnimation = layoutAnimations_[tag];
        auto &updateMap =
        surfaceManager.getUpdateMap(layoutAnimation.finalView->surfaceId);
        auto before = layoutAnimation.startView->layoutMetrics.frame;
        auto after = layoutAnimation.finalView->layoutMetrics.frame;
        auto x = before.origin.x + transitionProgress_*(after.origin.x - before.origin.x);
        auto y = before.origin.y + transitionProgress_*(after.origin.y - before.origin.y);
        auto width = before.size.width + transitionProgress_*(after.size.width - before.size.width);
        auto height = before.size.height + transitionProgress_*(after.size.height - before.size.height);
        
        updateMap.insert_or_assign(tag, UpdateValues{nullptr, {x,y,width,height}});
      }
    }
    
    
    if (transitionState_ == START){
      transitionState_ = ACTIVE;
    } else if (transitionState_ == END || transitionState_ == CANCELLED){
      for (auto tag: activeTransitions_){
        sharedContainersToRemove_.push_back(tag);
        tagsToRestore_.push_back(restoreMap_[tag]);
      }
      sharedTransitionManager_->groups_.clear();
      activeTransitions_.clear();
      transitionState_ = NONE;
    }
  }
}

void LayoutAnimationsProxy::updateLightTree(const ShadowViewMutationList &mutations) const {
  for (auto &mutation: mutations){
    switch (mutation.type) {
      case ShadowViewMutation::Update:{
        auto& node = lightNodes_[mutation.newChildShadowView.tag];
        //              node->previous = mutation.oldChildShadowView;
        node->current = mutation.newChildShadowView;
        if (!strcmp(node->current.componentName, "ScrollView")){
          //                auto state = std::static_pointer_cast<const ScrollViewShadowNode::ConcreteState>(node->current.state);
          //                auto data = state->getData();
          //                LOG(INFO) << node->current.tag << " update content offset:" << data.contentOffset.x << " " << data.contentOffset.y;
        }
        break;
      }
      case ShadowViewMutation::Create:{
        auto& node = lightNodes_[mutation.newChildShadowView.tag];
        node = std::make_shared<LightNode>();
        node->current = mutation.newChildShadowView;
        break;
      }
      case ShadowViewMutation::Delete:{
        //            lightNodes_.erase(mutation.oldChildShadowView.tag);
        break;
      }
      case ShadowViewMutation::Insert:{
        transferConfigFromNativeID(
                                   mutation.newChildShadowView.props->nativeId,
                                   mutation.newChildShadowView.tag);
        auto& node = lightNodes_[mutation.newChildShadowView.tag];
        auto& parent = lightNodes_[mutation.parentTag];
        parent->children.insert(parent->children.begin()+mutation.index, node);
        node->parent = parent;
      }
      case ShadowViewMutation::Remove:{
        //              auto& node = lightNodes_[mutation.oldChildShadowView.tag];
        auto& parent = lightNodes_[mutation.parentTag];
        if (parent->children[mutation.index]->current.tag == mutation.oldChildShadowView.tag){
          parent->children.erase(parent->children.begin()+mutation.index);
          //              node->parent.reset();
        }
      }
      default:
        break;
    }
  }
}

void LayoutAnimationsProxy::handleSharedTransitionsStart(const LightNode::Unshared &afterTopScreen, const LightNode::Unshared &beforeTopScreen, ShadowViewMutationList &filteredMutations, const ShadowViewMutationList &mutations, const PropsParserContext &propsParserContext, SurfaceId surfaceId) const {
  {
    ReanimatedSystraceSection s1("moj narzut 2");
    
    if (beforeTopScreen && afterTopScreen && beforeTopScreen->current.tag != afterTopScreen->current.tag){
      LOG(INFO) << "different tags";
      LOG(INFO) << "start transition: " << beforeTopScreen->current.tag << " -> " << afterTopScreen->current.tag;
      
      for (auto& [sharedTag, transition]: transitions_){
        LOG(INFO) << "sharedTag: " << sharedTag;
        const auto& [before, after] = transition.snapshot;
        const auto& [beforeParentTag, afterParentTag] = transition.parentTag;
        
        auto fakeTag = sharedTransitionManager_->groups_[sharedTag].fakeTag;
        auto shouldCreateContainer = (fakeTag == -1 || !layoutAnimations_.contains(fakeTag));
        if (shouldCreateContainer){
          auto& root = lightNodes_[surfaceId];
          ShadowView s = before;
          s.tag = myTag;
          filteredMutations.push_back(ShadowViewMutation::CreateMutation(s));
          filteredMutations.push_back(ShadowViewMutation::InsertMutation(surfaceId, s, root->children.size()));
          filteredMutations.push_back(ShadowViewMutation::UpdateMutation(after, after, afterParentTag));
          auto m = ShadowViewMutation::UpdateMutation(after, after, afterParentTag);
          m = ShadowViewMutation::UpdateMutation(after, *cloneViewWithoutOpacity(m, propsParserContext), afterParentTag);
          filteredMutations.push_back(m);
          auto node = std::make_shared<LightNode>();
          node->current = s;
          lightNodes_[myTag] = node;
          root->children.push_back(node);
          fakeTag = myTag;
        }
        layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[fakeTag] = layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[before.tag];
        ShadowView copy = after;
        copy.tag = fakeTag;
        auto copy2 = before;
        copy2.tag = fakeTag;
        startSharedTransition(fakeTag, copy2, copy, surfaceId);
        restoreMap_[fakeTag] = after.tag;
        if (shouldCreateContainer){
          sharedTransitionManager_->groups_[sharedTag].fakeTag = myTag;
          myTag+=2;
        }
      }
    } else if (mutations.size() && beforeTopScreen && afterTopScreen && beforeTopScreen->current.tag == afterTopScreen->current.tag){
      LOG(INFO) << "same tag";
      for (auto& [sharedTag, transition]: transitions_){
        const auto& [_, after] = transition.snapshot;
        
        auto copy = after;
        auto fakeTag = sharedTransitionManager_->groups_[sharedTag].fakeTag;
        copy.tag = fakeTag;
        if (!layoutAnimations_.contains(fakeTag)){
          continue;
        }
        auto& la = layoutAnimations_[fakeTag];
        if (la.finalView->layoutMetrics != copy.layoutMetrics){
          startSharedTransition(fakeTag, copy, copy, surfaceId);
        }
      }
    }
  }
}

void LayoutAnimationsProxy::cleanupSharedTransitions(ShadowViewMutationList &filteredMutations, const PropsParserContext &propsParserContext, SurfaceId surfaceId) const {
  for (auto& tag: tagsToRestore_){
    auto& node = lightNodes_[tag];
    if (node){
      auto view = node->current;
      auto parentTag = node->parent.lock()->current.tag;
      auto m = ShadowViewMutation::UpdateMutation(view, view, parentTag);
      m = ShadowViewMutation::UpdateMutation(*cloneViewWithoutOpacity(m, propsParserContext), *cloneViewWithOpacity(m, propsParserContext), parentTag);
      filteredMutations.push_back(m);
    }
  }
  tagsToRestore_.clear();
  
  for (auto& tag: sharedContainersToRemove_){
    auto root = lightNodes_[surfaceId];
    for (int i=0; i< root->children.size(); i++){
      auto& child = root->children[i];
      if (child->current.tag == tag){
        filteredMutations.push_back(ShadowViewMutation::RemoveMutation(surfaceId, child->current, i));
        filteredMutations.push_back(ShadowViewMutation::DeleteMutation(child->current));
        LOG(INFO) << "delete container " << tag;
        root->children.erase(root->children.begin()+i);
      }
    }
  }
  sharedContainersToRemove_.clear();
}

void LayoutAnimationsProxy::hideTransitioningViews(int index, ShadowViewMutationList &filteredMutations, const PropsParserContext &propsParserContext) const {
  for (auto& [sharedTag, transition]: transitions_){
    const auto& shadowView = transition.snapshot[index];
    const auto& parentTag = transition.parentTag[index];
    auto m = ShadowViewMutation::UpdateMutation(shadowView, shadowView, parentTag);
    m = ShadowViewMutation::UpdateMutation(shadowView, *cloneViewWithoutOpacity(m, propsParserContext), parentTag);
    filteredMutations.push_back(m);
  }
}


std::optional<SurfaceId> LayoutAnimationsProxy::progressLayoutAnimation(
    int tag,
    const jsi::Object &newStyle) {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "progress layout animation for tag " << tag << std::endl;
#endif
  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
  auto layoutAnimationIt = layoutAnimations_.find(tag);

  if (layoutAnimationIt == layoutAnimations_.end()) {
    return {};
  }

  auto &layoutAnimation = layoutAnimationIt->second;

  maybeRestoreOpacity(layoutAnimation, newStyle);

  auto rawProps =
      std::make_shared<RawProps>(uiRuntime_, jsi::Value(uiRuntime_, newStyle));

  PropsParserContext propsParserContext{
      layoutAnimation.finalView->surfaceId, *contextContainer_};
#ifdef ANDROID
  rawProps = std::make_shared<RawProps>(folly::dynamic::merge(
      layoutAnimation.finalView->props->rawProps, (folly::dynamic)*rawProps));
#endif
  auto newProps =
      getComponentDescriptorForShadowView(*layoutAnimation.finalView)
          .cloneProps(
              propsParserContext,
              layoutAnimation.finalView->props,
              std::move(*rawProps));
  auto &updateMap =
      surfaceManager.getUpdateMap(layoutAnimation.finalView->surfaceId);
  updateMap.insert_or_assign(
      tag, UpdateValues{newProps, Frame(uiRuntime_, newStyle)});

  return layoutAnimation.finalView->surfaceId;
}

std::optional<SurfaceId> LayoutAnimationsProxy::endLayoutAnimation(
    int tag,
    bool shouldRemove) {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "end layout animation for " << tag << " - should remove "
            << shouldRemove << std::endl;
#endif
  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
  auto layoutAnimationIt = layoutAnimations_.find(tag);

  if (layoutAnimationIt == layoutAnimations_.end()) {
    return {};
  }

  auto &layoutAnimation = layoutAnimationIt->second;

  // multiple layout animations can be triggered for a view
  // one after the other, so we need to keep count of how many
  // were actually triggered, so that we don't cleanup necessary
  // structures too early
  if (layoutAnimation.count > 1) {
    layoutAnimation.count--;
    return {};
  }

  auto surfaceId = layoutAnimation.finalView->surfaceId;
  auto &updateMap = surfaceManager.getUpdateMap(surfaceId);
  layoutAnimations_.erase(tag);
  updateMap.erase(tag);
  
  auto sharedTag = sharedTransitionManager_->tagToName_[tag];
  sharedTransitionManager_->groups_.erase(sharedTag);
  
  sharedContainersToRemove_.push_back(tag);
  tagsToRestore_.push_back(restoreMap_[tag]);

  if (!shouldRemove || !nodeForTag_.contains(tag)) {
    return surfaceId;
  }

  auto node = nodeForTag_[tag];
  auto mutationNode = std::static_pointer_cast<MutationNode>(node);
  mutationNode->state = DEAD;
  deadNodes.insert(mutationNode);

  return surfaceId;
}

std::optional<SurfaceId> LayoutAnimationsProxy::onTransitionProgress(int tag, double progress, bool isClosing, bool isGoingForward, bool isSwiping){
  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
  transitionUpdated_ = true;
//  LOG(INFO) << "notifyTransitionProgress ("<< tag <<"): " << progress << ", closing: " << isClosing << ", goingForward: " << isGoingForward << ", isSwiping: " <<isSwiping;
  
  if (isSwiping && !isClosing){
    transitionProgress_ = progress;
    if (transitionState_ == NONE && progress < 1){
      transitionState_ = START;
      transitionTag_ = tag;
    }
//    else if (transitionState_ == ACTIVE && progress < eps){
//      transitionState_ = CANCELLED;
//    }
    else if (transitionState_ == ACTIVE && progress == 1) {
      transitionState_ = END;
    }
    return 1;
  }
  return {};
}

std::optional<SurfaceId> LayoutAnimationsProxy::onGestureCancel(){
  auto lock = std::unique_lock<std::recursive_mutex>(mutex);
  if (transitionState_){
    transitionState_ = CANCELLED;
    return 1;
  }
  return {};
}

/**
 Organizes removed views into a tree structure, allowing for convenient
 traversals and index maintenance
 */
void LayoutAnimationsProxy::parseRemoveMutations(
    std::unordered_map<Tag, ShadowView> &movedViews,
    ShadowViewMutationList &mutations,
    std::vector<std::shared_ptr<MutationNode>> &roots) const {
  std::set<Tag> deletedViews;
  std::unordered_map<Tag, std::vector<std::shared_ptr<MutationNode>>>
      childrenForTag, unflattenedChildrenForTag;

  std::vector<std::shared_ptr<MutationNode>> mutationNodes;

  // iterate from the end, so that parents appear before children
  for (auto it = mutations.rbegin(); it != mutations.rend(); it++) {
    auto &mutation = *it;
    if (mutation.type == ShadowViewMutation::Delete) {
      deletedViews.insert(mutation.oldChildShadowView.tag);
    }
    if (mutation.type == ShadowViewMutation::Remove) {
      updateIndexForMutation(mutation);
      auto tag = mutation.oldChildShadowView.tag;
#if REACT_NATIVE_MINOR_VERSION >= 78
      auto parentTag = mutation.parentTag;
#else
      auto parentTag = mutation.parentShadowView.tag;
#endif // REACT_NATIVE_MINOR_VERSION >= 78
      auto unflattenedParentTag = parentTag; // temporary

      std::shared_ptr<MutationNode> mutationNode;
      std::shared_ptr<Node> node = nodeForTag_[tag],
                            parent = nodeForTag_[parentTag],
                            unflattenedParent =
                                nodeForTag_[unflattenedParentTag];

      if (!node) {
        mutationNode = std::make_shared<MutationNode>(mutation);
      } else {
        mutationNode =
            std::make_shared<MutationNode>(mutation, std::move(*node));
        for (auto &subNode : mutationNode->children) {
          subNode->parent = mutationNode;
        }
        for (auto &subNode : mutationNode->unflattenedChildren) {
          subNode->unflattenedParent = mutationNode;
        }
      }
      if (!deletedViews.contains(mutation.oldChildShadowView.tag)) {
        mutationNode->state = MOVED;
        movedViews.insert_or_assign(
            mutation.oldChildShadowView.tag, mutation.oldChildShadowView);
      }
      nodeForTag_[tag] = mutationNode;

      if (!parent) {
        parent = std::make_shared<Node>(parentTag);
        nodeForTag_[parentTag] = parent;
      }

      if (!unflattenedParent) {
        if (parentTag == unflattenedParentTag) {
          unflattenedParent = parent;
        } else {
          unflattenedParent = std::make_shared<Node>(unflattenedParentTag);
          nodeForTag_[unflattenedParentTag] = unflattenedParent;
        }
      }

      mutationNodes.push_back(mutationNode);

      childrenForTag[parentTag].push_back(mutationNode);
      unflattenedChildrenForTag[unflattenedParentTag].push_back(mutationNode);
      mutationNode->parent = parent;
      mutationNode->unflattenedParent = unflattenedParent;
    }
    if (mutation.type == ShadowViewMutation::Update &&
        movedViews.contains(mutation.newChildShadowView.tag)) {
      auto node = nodeForTag_[mutation.newChildShadowView.tag];
      auto mutationNode = std::static_pointer_cast<MutationNode>(node);
      mutationNode->mutation.oldChildShadowView = mutation.oldChildShadowView;
      movedViews[mutation.newChildShadowView.tag] = mutation.oldChildShadowView;
    }
  }

  for (auto &[parentTag, children] : childrenForTag) {
    auto &parent = nodeForTag_[parentTag];
    parent->insertChildren(children);
    for (auto &child : children) {
      child->parent = parent;
    }
  }
  for (auto &[unflattenedParentTag, children] : unflattenedChildrenForTag) {
    auto &unflattenedParent = nodeForTag_[unflattenedParentTag];
    unflattenedParent->insertUnflattenedChildren(children);
    for (auto &child : children) {
      child->unflattenedParent = unflattenedParent;
    }
  }

  for (auto &mutationNode : mutationNodes) {
    if (!mutationNode->unflattenedParent->isMutationMode()) {
      roots.push_back(mutationNode);
    }
  }
}

void LayoutAnimationsProxy::handleRemovals(
    ShadowViewMutationList &filteredMutations,
    std::vector<std::shared_ptr<MutationNode>> &roots) const {
  // iterate from the end, so that children
  // with higher indices appear first in the mutations list
  for (auto it = roots.rbegin(); it != roots.rend(); it++) {
    auto &node = *it;
    if (!startAnimationsRecursively(
            node, true, true, false, filteredMutations)) {
      filteredMutations.push_back(node->mutation);
      node->unflattenedParent->removeChildFromUnflattenedTree(node); //???
      if (node->state != MOVED) {
        maybeCancelAnimation(node->tag);
        filteredMutations.push_back(ShadowViewMutation::DeleteMutation(
            node->mutation.oldChildShadowView));
        
//        auto tag = node->tag;
        
//        if (layoutAnimationsManager_->hasLayoutAnimation(tag, SHARED_ELEMENT_TRANSITION)){
//          auto p = sharedTransitionManager_->remove(tag);
//          if (p){
//            const auto& [before, after] = *p;
//            ShadowView s = before;
//            s.tag = myTag;
//            s.layoutMetrics.frame.origin.y += 100;
//            filteredMutations.push_back(ShadowViewMutation::CreateMutation(s));
//            filteredMutations.push_back(ShadowViewMutation::InsertMutation(1, s, 1));
//            layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[myTag] = layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[before.tag];
//            ShadowView copy = after;
//            copy.tag = myTag;
//            auto copy2 = before;
//            copy2.tag = myTag;
//            copy.layoutMetrics.frame.origin.y += 100;
//            copy2.layoutMetrics.frame.origin.y += 100;
//            startSharedTransition(myTag, copy2, copy);
//            myTag+=2;
//            continue;
//          }
//        }
        
        nodeForTag_.erase(node->tag);
#ifdef LAYOUT_ANIMATIONS_LOGS
        LOG(INFO) << "delete " << node->tag << std::endl;
#endif
      }
    }
  }

  for (auto node : deadNodes) {
    if (node->state != DELETED) {
      endAnimationsRecursively(node, filteredMutations);
      maybeDropAncestors(node->unflattenedParent, node, filteredMutations);
    }
  }
  deadNodes.clear();
}

void LayoutAnimationsProxy::handleUpdatesAndEnterings(
    ShadowViewMutationList &filteredMutations,
    const std::unordered_map<Tag, ShadowView> &movedViews,
    ShadowViewMutationList &mutations,
    const PropsParserContext &propsParserContext,
    SurfaceId surfaceId) const {
  std::unordered_map<Tag, ShadowView> oldShadowViewsForReparentings;
  for (auto &mutation : mutations) {
    maybeUpdateWindowDimensions(mutation, surfaceId);

    Tag tag = mutation.type == ShadowViewMutation::Type::Create ||
            mutation.type == ShadowViewMutation::Type::Insert
        ? mutation.newChildShadowView.tag
        : mutation.oldChildShadowView.tag;

    switch (mutation.type) {
      case ShadowViewMutation::Type::Create: {
        filteredMutations.push_back(mutation);
        break;
      }
      case ShadowViewMutation::Type::Insert: {
        updateIndexForMutation(mutation);

#if REACT_NATIVE_MINOR_VERSION >= 78
        const auto parentTag = mutation.parentTag;
        const auto mutationParent = parentTag;
#else
        const auto parentTag = mutation.parentShadowView.tag;
        const auto mutationParent = mutation.parentShadowView;
#endif // REACT_NATIVE_MINOR_VERSION >= 78
        if (nodeForTag_.contains(parentTag)) {
          nodeForTag_[parentTag]->applyMutationToIndices(mutation);
        }
        
//        if (layoutAnimationsManager_->hasLayoutAnimation(tag, SHARED_ELEMENT_TRANSITION)){
//          auto previousView = sharedTransitionManager_->add(mutation.newChildShadowView);
//          if (previousView){
//            ShadowView s = *previousView;
//            s.tag = myTag;
//            s.layoutMetrics.frame.origin.y += 100;
//            filteredMutations.push_back(ShadowViewMutation::CreateMutation(s));
//            filteredMutations.push_back(ShadowViewMutation::InsertMutation(1, s, 1));
//            layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[myTag] = layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[previousView->tag];
//            ShadowView copy = mutation.newChildShadowView;
//            copy.tag = myTag;
//            previousView->tag = myTag;
//            copy.layoutMetrics.frame.origin.y += 100;
//            previousView->layoutMetrics.frame.origin.y += 100;
//            startSharedTransition(myTag, *previousView, copy);
//            myTag+=2;
//            std::shared_ptr<ShadowView> newView =
//                cloneViewWithoutOpacity(mutation, propsParserContext);
//            mutation.newChildShadowView = *newView;
//            filteredMutations.push_back(mutation);
//            continue;
//          }
//        }

        if (movedViews.contains(tag)) {
          auto layoutAnimationIt = layoutAnimations_.find(tag);
          if (layoutAnimationIt == layoutAnimations_.end()) {
            if (oldShadowViewsForReparentings.contains(tag)) {
              filteredMutations.push_back(ShadowViewMutation::InsertMutation(
                  mutationParent,
                  oldShadowViewsForReparentings[tag],
                  mutation.index));
            } else {
              filteredMutations.push_back(mutation);
            }
            continue;
          }

          auto oldView = *layoutAnimationIt->second.currentView;
          filteredMutations.push_back(ShadowViewMutation::InsertMutation(
              mutationParent, oldView, mutation.index));
          continue;
        }

        if (!layoutAnimationsManager_->hasLayoutAnimation(tag, ENTERING)) {
          filteredMutations.push_back(mutation);
          continue;
        }

        startEnteringAnimation(tag, mutation);
        filteredMutations.push_back(mutation);

        // temporarily set opacity to 0 to prevent flickering on android
        std::shared_ptr<ShadowView> newView =
            cloneViewWithoutOpacity(mutation, propsParserContext);

        filteredMutations.push_back(ShadowViewMutation::UpdateMutation(
            mutation.newChildShadowView, *newView, mutationParent));
        break;
      }

      case ShadowViewMutation::Type::Update: {
        auto shouldAnimate = hasLayoutChanged(mutation);
        if (!layoutAnimationsManager_->hasLayoutAnimation(tag, LAYOUT) ||
            (!shouldAnimate && !layoutAnimations_.contains(tag))) {
          // We should cancel any ongoing animation here to ensure that the
          // proper final state is reached for this view However, due to how
          // RNSScreens handle adding headers (a second commit is triggered to
          // offset all the elements by the header height) this would lead to
          // all entering animations being cancelled when a screen with a header
          // is pushed onto a stack
          // TODO: find a better solution for this problem
          filteredMutations.push_back(mutation);
          continue;
        } else if (!shouldAnimate) {
          updateOngoingAnimationTarget(tag, mutation);
          continue;
        }

        // store the oldChildShadowView, so that we can use this ShadowView when
        // the view is inserted
        oldShadowViewsForReparentings[tag] = mutation.oldChildShadowView;
        startLayoutAnimation(tag, mutation);
        break;
      }

      case ShadowViewMutation::Type::Remove:
      case ShadowViewMutation::Type::Delete: {
        break;
      }

      default:
        filteredMutations.push_back(mutation);
    }
  }
}

void LayoutAnimationsProxy::addOngoingAnimations(
    SurfaceId surfaceId,
    ShadowViewMutationList &mutations) const {
  auto &updateMap = surfaceManager.getUpdateMap(surfaceId);
  for (auto &[tag, updateValues] : updateMap) {
    auto layoutAnimationIt = layoutAnimations_.find(tag);

    if (layoutAnimationIt == layoutAnimations_.end()) {
      continue;
    }

    auto &layoutAnimation = layoutAnimationIt->second;

    auto newView = std::make_shared<ShadowView>(*layoutAnimation.finalView);
    if (updateValues.newProps){
      newView->props = updateValues.newProps;
    }
    updateLayoutMetrics(newView->layoutMetrics, updateValues.frame);
    
    LOG(INFO) << "(addOngoing) " << tag;

    mutations.push_back(ShadowViewMutation::UpdateMutation(
        *layoutAnimation.currentView,
        *newView,
#if REACT_NATIVE_MINOR_VERSION >= 78
        layoutAnimation.parentTag
#else
        *layoutAnimation.parentView
#endif // REACT_NATIVE_MINOR_VERSION >= 78
        ));
    layoutAnimation.currentView = newView;
  }
  updateMap.clear();
}

void LayoutAnimationsProxy::endAnimationsRecursively(
    std::shared_ptr<MutationNode> node,
    ShadowViewMutationList &mutations) const {
  maybeCancelAnimation(node->tag);
  node->state = DELETED;
  // iterate from the end, so that children
  // with higher indices appear first in the mutations list
  for (auto it = node->unflattenedChildren.rbegin();
       it != node->unflattenedChildren.rend();
       it++) {
    auto &subNode = *it;
    if (subNode->state != DELETED) {
      endAnimationsRecursively(subNode, mutations);
    }
  }
  mutations.push_back(node->mutation);
  nodeForTag_.erase(node->tag);
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "delete " << node->tag << std::endl;
#endif
  mutations.push_back(
      ShadowViewMutation::DeleteMutation(node->mutation.oldChildShadowView));
}

void LayoutAnimationsProxy::maybeDropAncestors(
    std::shared_ptr<Node> parent,
    std::shared_ptr<MutationNode> child,
    ShadowViewMutationList &cleanupMutations) const {
  parent->removeChildFromUnflattenedTree(child);
  if (!parent->isMutationMode()) {
    return;
  }

  auto node = std::static_pointer_cast<MutationNode>(parent);

  if (node->children.size() == 0 && node->state != ANIMATING) {
    nodeForTag_.erase(node->tag);
    cleanupMutations.push_back(node->mutation);
    maybeCancelAnimation(node->tag);
#ifdef LAYOUT_ANIMATIONS_LOGS
    LOG(INFO) << "delete " << node->tag << std::endl;
#endif
    cleanupMutations.push_back(
        ShadowViewMutation::DeleteMutation(node->mutation.oldChildShadowView));
    maybeDropAncestors(node->unflattenedParent, node, cleanupMutations);
  }
}

const ComponentDescriptor &
LayoutAnimationsProxy::getComponentDescriptorForShadowView(
    const ShadowView &shadowView) const {
  return componentDescriptorRegistry_->at(shadowView.componentHandle);
}

bool LayoutAnimationsProxy::startAnimationsRecursively(
    std::shared_ptr<MutationNode> node,
    bool shouldRemoveSubviewsWithoutAnimations,
    bool shouldAnimate,
    bool isScreenPop,
    ShadowViewMutationList &mutations) const {
  if (isRNSScreen(node)) {
    isScreenPop = true;
  }

  shouldAnimate = !isScreenPop &&
      layoutAnimationsManager_->shouldAnimateExiting(node->tag, shouldAnimate);

  bool hasExitAnimation = shouldAnimate &&
      layoutAnimationsManager_->hasLayoutAnimation(
          node->tag, LayoutAnimationType::EXITING);
  bool hasAnimatedChildren = false;

  shouldRemoveSubviewsWithoutAnimations =
      shouldRemoveSubviewsWithoutAnimations && !hasExitAnimation;
  std::vector<std::shared_ptr<MutationNode>> toBeRemoved;

  // iterate from the end, so that children
  // with higher indices appear first in the mutations list
  for (auto it = node->unflattenedChildren.rbegin();
       it != node->unflattenedChildren.rend();
       it++) {
    auto &subNode = *it;
#ifdef LAYOUT_ANIMATIONS_LOGS
    LOG(INFO) << "child " << subNode->tag << " "
              << " " << shouldAnimate << " "
              << shouldRemoveSubviewsWithoutAnimations << std::endl;
#endif
    if (subNode->state != UNDEFINED && subNode->state != MOVED) {
      if (shouldAnimate && subNode->state != DEAD) {
        hasAnimatedChildren = true;
      } else {
        endAnimationsRecursively(subNode, mutations);
        toBeRemoved.push_back(subNode);
      }
    } else if (startAnimationsRecursively(
                   subNode,
                   shouldRemoveSubviewsWithoutAnimations,
                   shouldAnimate,
                   isScreenPop,
                   mutations)) {
#ifdef LAYOUT_ANIMATIONS_LOGS
      LOG(INFO) << "child " << subNode->tag
                << " start animations returned true " << std::endl;
#endif
      hasAnimatedChildren = true;
    } else if (subNode->state == MOVED) {
      mutations.push_back(subNode->mutation);
      toBeRemoved.push_back(subNode);
    } else if (shouldRemoveSubviewsWithoutAnimations) {
      
      maybeCancelAnimation(subNode->tag);
      mutations.push_back(subNode->mutation);
      toBeRemoved.push_back(subNode);
      subNode->state = DELETED;
      nodeForTag_.erase(subNode->tag);
#ifdef LAYOUT_ANIMATIONS_LOGS
      LOG(INFO) << "delete " << subNode->tag << std::endl;
#endif
      mutations.push_back(ShadowViewMutation::DeleteMutation(
          subNode->mutation.oldChildShadowView));
//      if (layoutAnimationsManager_->hasLayoutAnimation(subNode->tag, SHARED_ELEMENT_TRANSITION)){
//        auto p = sharedTransitionManager_->remove(subNode->tag);
//        if (p){
//          const auto& [before, after] = *p;
//          ShadowView s = before;
//          s.tag = myTag;
//          s.layoutMetrics.frame.origin.y += 100;
//          mutations.push_back(ShadowViewMutation::CreateMutation(s));
//          mutations.push_back(ShadowViewMutation::InsertMutation(1, s, 1));
//          layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[myTag] = layoutAnimationsManager_->getConfigsForType(LayoutAnimationType::SHARED_ELEMENT_TRANSITION)[before.tag];
//          ShadowView copy = after;
//          copy.tag = myTag;
//          auto copy2 = before;
//          copy2.tag = myTag;
//          copy.layoutMetrics.frame.origin.y += 100;
//          copy2.layoutMetrics.frame.origin.y += 100;
//          startSharedTransition(myTag, copy2, copy);
//          myTag+=2;
//          int c = 0;
//          findVisible(lightNodes_[subNode->tag], c);
//          continue;
//        }
//      }
    } else {
      subNode->state = WAITING;
    }
  }

  for (auto &subNode : toBeRemoved) {
    node->removeChildFromUnflattenedTree(subNode);
  }

  if (node->state == MOVED) {
    auto replacement = std::make_shared<Node>(*node);
    for (auto subNode : node->children) {
      subNode->parent = replacement;
    }
    for (auto subNode : node->unflattenedChildren) {
      subNode->unflattenedParent = replacement;
    }
    nodeForTag_[replacement->tag] = replacement;
    return false;
  }

  bool wantAnimateExit = hasExitAnimation || hasAnimatedChildren;

  if (hasExitAnimation) {
    node->state = ANIMATING;
    startExitingAnimation(node->tag, node->mutation);
  } else {
//    layoutAnimationsManager_->clearLayoutAnimationConfig(node->tag);
  }

  return wantAnimateExit;
}

void LayoutAnimationsProxy::updateIndexForMutation(
    ShadowViewMutation &mutation) const {
  if (mutation.index == -1) {
    return;
  }

#if REACT_NATIVE_MINOR_VERSION >= 78
  const auto parentTag = mutation.parentTag;
#else
  const auto parentTag = mutation.parentShadowView.tag;
#endif // REACT_NATIVE_MINOR_VERSION >= 78

  if (!nodeForTag_.contains(parentTag)) {
    return;
  }

  auto parent = nodeForTag_[parentTag];
  int size = 0, prevIndex = -1, offset = 0;

  for (auto &subNode : parent->children) {
    size += subNode->mutation.index - prevIndex - 1;
    if (mutation.index < size) {
      break;
    }
    offset++;
    prevIndex = subNode->mutation.index;
  }
#ifdef LAYOUT_ANIMATIONS_LOGS
  int tag = mutation.type == ShadowViewMutation::Insert
      ? mutation.newChildShadowView.tag
      : mutation.oldChildShadowView.tag;
  LOG(INFO) << "update index for " << tag << " in " << parentTag << ": "
            << mutation.index << " -> " << mutation.index + offset << std::endl;
#endif
  mutation.index += offset;
}

bool LayoutAnimationsProxy::shouldOverridePullTransaction() const {
  return true;
}

void LayoutAnimationsProxy::createLayoutAnimation(
    const ShadowViewMutation &mutation,
    ShadowView &oldView,
    const SurfaceId &surfaceId,
    const int tag) const {
  int count = 1;
  auto layoutAnimationIt = layoutAnimations_.find(tag);

  if (layoutAnimationIt != layoutAnimations_.end()) {
    auto &layoutAnimation = layoutAnimationIt->second;
    oldView = *layoutAnimation.currentView;
    count = layoutAnimation.count + 1;
  }

  auto finalView = std::make_shared<ShadowView>(
      mutation.type == ShadowViewMutation::Remove
          ? mutation.oldChildShadowView
          : mutation.newChildShadowView);
  auto currentView = std::make_shared<ShadowView>(oldView);
      auto startView =std::make_shared<ShadowView>(oldView);

#if REACT_NATIVE_MINOR_VERSION >= 78
  layoutAnimations_.insert_or_assign(
      tag,
   LayoutAnimation{finalView, currentView, startView, mutation.parentTag, {}, count});
#else
  auto parentView = std::make_shared<ShadowView>(mutation.parentShadowView);
  layoutAnimations_.insert_or_assign(
      tag, LayoutAnimation{finalView, currentView, parentView, {}, count});
#endif // REACT_NATIVE_MINOR_VERSION >= 78
}

void LayoutAnimationsProxy::startEnteringAnimation(
    const int tag,
    ShadowViewMutation &mutation) const {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "start entering animation for tag " << tag << std::endl;
#endif
  auto finalView = std::make_shared<ShadowView>(mutation.newChildShadowView);
  auto current = std::make_shared<ShadowView>(mutation.newChildShadowView);
#if REACT_NATIVE_MINOR_VERSION < 78
  auto parent = std::make_shared<ShadowView>(mutation.parentShadowView);
#endif

  auto &viewProps =
      static_cast<const ViewProps &>(*mutation.newChildShadowView.props);
  auto opacity = viewProps.opacity;

  uiScheduler_->scheduleOnUI([weakThis = weak_from_this(),
                              finalView,
                              current,
#if REACT_NATIVE_MINOR_VERSION < 78
                              parent,
#endif // REACT_NATIVE_MINOR_VERSION < 78
                              mutation,
                              opacity,
                              tag]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }

    Rect window{};
    {
      auto &mutex = strongThis->mutex;
      auto lock = std::unique_lock<std::recursive_mutex>(mutex);
      strongThis->layoutAnimations_.insert_or_assign(
          tag,
          LayoutAnimation{
              finalView,
              current,
            nullptr,
#if REACT_NATIVE_MINOR_VERSION >= 78
              mutation.parentTag,
#else
              parent,
#endif // REACT_NATIVE_MINOR_VERSION >= 78
              opacity});
      window = strongThis->surfaceManager.getWindow(
          mutation.newChildShadowView.surfaceId);
    }

    Snapshot values(mutation.newChildShadowView, window);
    auto &uiRuntime = strongThis->uiRuntime_;
    jsi::Object yogaValues(uiRuntime);
    yogaValues.setProperty(uiRuntime, "targetOriginX", values.x);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginX", values.x);
    yogaValues.setProperty(uiRuntime, "targetOriginY", values.y);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginY", values.y);
    yogaValues.setProperty(uiRuntime, "targetWidth", values.width);
    yogaValues.setProperty(uiRuntime, "targetHeight", values.height);
    yogaValues.setProperty(uiRuntime, "windowWidth", values.windowWidth);
    yogaValues.setProperty(uiRuntime, "windowHeight", values.windowHeight);
    strongThis->layoutAnimationsManager_->startLayoutAnimation(
        uiRuntime, tag, LayoutAnimationType::ENTERING, yogaValues);
  });
}

void LayoutAnimationsProxy::startExitingAnimation(
    const int tag,
    ShadowViewMutation &mutation) const {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "start exiting animation for tag " << tag << std::endl;
#endif
  auto surfaceId = mutation.oldChildShadowView.surfaceId;

  uiScheduler_->scheduleOnUI(
      [weakThis = weak_from_this(), tag, mutation, surfaceId]() {
        auto strongThis = weakThis.lock();
        if (!strongThis) {
          return;
        }

        auto oldView = mutation.oldChildShadowView;
        Rect window{};
        {
          auto &mutex = strongThis->mutex;
          auto lock = std::unique_lock<std::recursive_mutex>(mutex);
          strongThis->createLayoutAnimation(mutation, oldView, surfaceId, tag);
          window = strongThis->surfaceManager.getWindow(surfaceId);
        }

        Snapshot values(oldView, window);

        auto &uiRuntime = strongThis->uiRuntime_;
        jsi::Object yogaValues(uiRuntime);
        yogaValues.setProperty(uiRuntime, "currentOriginX", values.x);
        yogaValues.setProperty(uiRuntime, "currentGlobalOriginX", values.x);
        yogaValues.setProperty(uiRuntime, "currentOriginY", values.y);
        yogaValues.setProperty(uiRuntime, "currentGlobalOriginY", values.y);
        yogaValues.setProperty(uiRuntime, "currentWidth", values.width);
        yogaValues.setProperty(uiRuntime, "currentHeight", values.height);
        yogaValues.setProperty(uiRuntime, "windowWidth", values.windowWidth);
        yogaValues.setProperty(uiRuntime, "windowHeight", values.windowHeight);
        strongThis->layoutAnimationsManager_->startLayoutAnimation(
            uiRuntime, tag, LayoutAnimationType::EXITING, yogaValues);
        strongThis->layoutAnimationsManager_->clearLayoutAnimationConfig(tag);
      });
}

void LayoutAnimationsProxy::startLayoutAnimation(
    const int tag,
    const ShadowViewMutation &mutation) const {
#ifdef LAYOUT_ANIMATIONS_LOGS
  LOG(INFO) << "start layout animation for tag " << tag << std::endl;
#endif
  auto surfaceId = mutation.oldChildShadowView.surfaceId;

  uiScheduler_->scheduleOnUI([weakThis = weak_from_this(),
                              mutation,
                              surfaceId,
                              tag]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }

    auto oldView = mutation.oldChildShadowView;
    Rect window{};
    {
      auto &mutex = strongThis->mutex;
      auto lock = std::unique_lock<std::recursive_mutex>(mutex);
      strongThis->createLayoutAnimation(mutation, oldView, surfaceId, tag);
      window = strongThis->surfaceManager.getWindow(surfaceId);
    }

    Snapshot currentValues(oldView, window);
    Snapshot targetValues(mutation.newChildShadowView, window);

    auto &uiRuntime = strongThis->uiRuntime_;
    jsi::Object yogaValues(uiRuntime);
    yogaValues.setProperty(uiRuntime, "currentOriginX", currentValues.x);
    yogaValues.setProperty(uiRuntime, "currentGlobalOriginX", currentValues.x);
    yogaValues.setProperty(uiRuntime, "currentOriginY", currentValues.y);
    yogaValues.setProperty(uiRuntime, "currentGlobalOriginY", currentValues.y);
    yogaValues.setProperty(uiRuntime, "currentWidth", currentValues.width);
    yogaValues.setProperty(uiRuntime, "currentHeight", currentValues.height);
    yogaValues.setProperty(uiRuntime, "targetOriginX", targetValues.x);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginX", targetValues.x);
    yogaValues.setProperty(uiRuntime, "targetOriginY", targetValues.y);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginY", targetValues.y);
    yogaValues.setProperty(uiRuntime, "targetWidth", targetValues.width);
    yogaValues.setProperty(uiRuntime, "targetHeight", targetValues.height);
    yogaValues.setProperty(uiRuntime, "windowWidth", targetValues.windowWidth);
    yogaValues.setProperty(
        uiRuntime, "windowHeight", targetValues.windowHeight);
    strongThis->layoutAnimationsManager_->startLayoutAnimation(
        uiRuntime, tag, LayoutAnimationType::LAYOUT, yogaValues);
  });
}

void LayoutAnimationsProxy::startSharedTransition(const int tag, const ShadowView &before, const ShadowView &after, SurfaceId surfaceId) const{

  uiScheduler_->scheduleOnUI([weakThis = weak_from_this(),
                              before,
                              after,
                              surfaceId,
                              tag]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }

    auto oldView = before;
    Rect window{};
    {
      auto &mutex = strongThis->mutex;
      auto lock = std::unique_lock<std::recursive_mutex>(mutex);
      strongThis->createLayoutAnimation(ShadowViewMutation::InsertMutation(surfaceId, after, 1), oldView, surfaceId, tag);
      window = strongThis->surfaceManager.getWindow(surfaceId);
    }

    Snapshot currentValues(oldView, window);
    Snapshot targetValues(after, window);

    auto &uiRuntime = strongThis->uiRuntime_;
    jsi::Object yogaValues(uiRuntime);
    yogaValues.setProperty(uiRuntime, "currentOriginX", currentValues.x);
    yogaValues.setProperty(uiRuntime, "currentGlobalOriginX", currentValues.x);
    yogaValues.setProperty(uiRuntime, "currentOriginY", currentValues.y);
    yogaValues.setProperty(uiRuntime, "currentGlobalOriginY", currentValues.y);
    yogaValues.setProperty(uiRuntime, "currentWidth", currentValues.width);
    yogaValues.setProperty(uiRuntime, "currentHeight", currentValues.height);
    yogaValues.setProperty(uiRuntime, "targetOriginX", targetValues.x);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginX", targetValues.x);
    yogaValues.setProperty(uiRuntime, "targetOriginY", targetValues.y);
    yogaValues.setProperty(uiRuntime, "targetGlobalOriginY", targetValues.y);
    yogaValues.setProperty(uiRuntime, "targetWidth", targetValues.width);
    yogaValues.setProperty(uiRuntime, "targetHeight", targetValues.height);
    yogaValues.setProperty(uiRuntime, "windowWidth", targetValues.windowWidth);
    yogaValues.setProperty(uiRuntime, "windowHeight", targetValues.windowHeight);
    strongThis->layoutAnimationsManager_->startLayoutAnimation(uiRuntime, tag, LayoutAnimationType::SHARED_ELEMENT_TRANSITION, yogaValues);
  });
}

void LayoutAnimationsProxy::startProgressTransition(const int tag, const ShadowView &before, const ShadowView &after, SurfaceId surfaceId) const{

  uiScheduler_->scheduleOnUI([weakThis = weak_from_this(),
                              before,
                              after,
                              surfaceId,
                              tag]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }

    auto oldView = before;
    Rect window{};
    {
      auto &mutex = strongThis->mutex;
      auto lock = std::unique_lock<std::recursive_mutex>(mutex);
      strongThis->createLayoutAnimation(ShadowViewMutation::InsertMutation(surfaceId, after, 1), oldView, surfaceId, tag);
      window = strongThis->surfaceManager.getWindow(surfaceId);
    }
  });
}

void LayoutAnimationsProxy::updateOngoingAnimationTarget(
    const int tag,
    const ShadowViewMutation &mutation) const {
  layoutAnimations_[tag].finalView =
      std::make_shared<ShadowView>(mutation.newChildShadowView);
}

void LayoutAnimationsProxy::maybeCancelAnimation(const int tag) const {
  if (!layoutAnimations_.contains(tag)) {
    return;
  }
  layoutAnimations_.erase(tag);
  uiScheduler_->scheduleOnUI([weakThis = weak_from_this(), tag]() {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }

    auto &uiRuntime = strongThis->uiRuntime_;
    strongThis->layoutAnimationsManager_->cancelLayoutAnimation(uiRuntime, tag);
  });
}

void LayoutAnimationsProxy::transferConfigFromNativeID(
    const std::string nativeIdString,
    const int tag) const {
  if (nativeIdString.empty()) {
    return;
  }
  try {
    auto nativeId = stoi(nativeIdString);
    layoutAnimationsManager_->transferConfigFromNativeID(nativeId, tag);
  } catch (std::invalid_argument) {
  } catch (std::out_of_range) {
  }
}

// When entering animations start, we temporarily set opacity to 0
// so that we can immediately insert the view at the right position
// and schedule the animation on the UI thread
std::shared_ptr<ShadowView> LayoutAnimationsProxy::cloneViewWithoutOpacity(
    facebook::react::ShadowViewMutation &mutation,
    const PropsParserContext &propsParserContext) const {
  auto newView = std::make_shared<ShadowView>(mutation.newChildShadowView);
  folly::dynamic opacity = folly::dynamic::object("opacity", 0);
  auto newProps = getComponentDescriptorForShadowView(*newView).cloneProps(
      propsParserContext, newView->props, RawProps(opacity));
  newView->props = newProps;
  return newView;
}

    std::shared_ptr<ShadowView> LayoutAnimationsProxy::cloneViewWithOpacity(
            facebook::react::ShadowViewMutation &mutation,
            const PropsParserContext &propsParserContext) const {
        auto newView = std::make_shared<ShadowView>(mutation.newChildShadowView);
        folly::dynamic opacity = folly::dynamic::object("opacity", 1);
        auto newProps = getComponentDescriptorForShadowView(*newView).cloneProps(
                propsParserContext, newView->props, RawProps(opacity));
        newView->props = newProps;
        return newView;
    }

void LayoutAnimationsProxy::maybeRestoreOpacity(
    LayoutAnimation &layoutAnimation,
    const jsi::Object &newStyle) const {
  if (layoutAnimation.opacity && !newStyle.hasProperty(uiRuntime_, "opacity")) {
    newStyle.setProperty(
        uiRuntime_, "opacity", jsi::Value(*layoutAnimation.opacity));
    layoutAnimation.opacity.reset();
  }
}

void LayoutAnimationsProxy::maybeUpdateWindowDimensions(
    facebook::react::ShadowViewMutation &mutation,
    SurfaceId surfaceId) const {
  if (mutation.type == ShadowViewMutation::Update &&
      !std::strcmp(
          mutation.oldChildShadowView.componentName, RootComponentName)) {
    surfaceManager.updateWindow(
        surfaceId,
        mutation.newChildShadowView.layoutMetrics.frame.size.width,
        mutation.newChildShadowView.layoutMetrics.frame.size.height);
  }
}

} // namespace reanimated
