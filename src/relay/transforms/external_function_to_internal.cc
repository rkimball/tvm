/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file visualize_graph.cc
 */

#include <tvm/relay/analysis.h>
#include <tvm/relay/attrs/annotation.h>
#include <tvm/relay/attrs/transform.h>
#include <tvm/relay/expr_functor.h>
#include <tvm/relay/interpreter.h>
#include <tvm/relay/op.h>
#include <tvm/relay/op_attr_types.h>
#include <tvm/relay/transform.h>
#include <tvm/runtime/container.h>
#include <tvm/runtime/ndarray.h>
#include <tvm/runtime/object.h>

#include <fstream>
#include <unordered_map>

#include "../../printer/text_printer.h"
#include "../../support/utils.h"
#include "../ir/indexed_graph.h"
#include "pattern_utils.h"

namespace tvm {
namespace relay {

class FunctionInternalizer : public MixedModeMutator {
 public:
  using MixedModeMutator::VisitExpr_;
  explicit FunctionInternalizer(IRModule module) : module_(module) {}

  Expr VisitExpr_(const FunctionNode* func_node) override {
    std::cout << __FILE__ << " " << __LINE__ << std::endl;
    if (func_node->attrs.defined() && func_node->attrs->dict.count(attr::kCompiler) != 0) {
      std::cout << __FILE__ << " " << __LINE__ << " strip compiler attrs" << std::endl;
      return Function(func_node->params, func_node->body, func_node->ret_type,
                      func_node->type_params, DictAttrs(), func_node->span);
    } else {
      return ExprMutator::VisitExpr_(func_node);
    }
  }
  Expr VisitExpr_(const GlobalVarNode* gvar) override {
    std::cout << __FILE__ << " " << __LINE__ << " gvar" << std::endl;
    // if (gvar->op.attrs.defined() && gvar->attrs->dict.count(attr::kCompiler) != 0) {
    //   std::cout << __FILE__ << " " << __LINE__ << " gvar has compiler attrs" << std::endl;
    // }
    return ExprMutator::VisitExpr_(gvar);
  }

  Expr Rewrite_(const CallNode* pre, const Expr& post) override {
    Expr rc = post;
    const CallNode* call_node = post.as<CallNode>();
    if (call_node->op.as<OpNode>()) {
      // std::cout << __FILE__ << " " << __LINE__ << " call is Op" << std::endl;
      // int device_type = get_placement_(GetRef<Expr>(call_node));
      // if (device_type > 0) {
      //   rc = relay::op::annotation::on_device(post, device_type);
      // }
    } else if (call_node->op.as<FunctionNode>()) {
      std::cout << __FILE__ << " " << __LINE__ << " call is FunctionNode" << std::endl;
    } else {
      std::cout << __FILE__ << " " << __LINE__ << " call is not Op" << std::endl;
      // std::cout << AsText(post, false) << std::endl;
    }
    // else if (call_node->op.as<GlobalVarNode>()) {
    //   std::cout << __FILE__ << " " << __LINE__ << " op is GlobalVarNode" << std::endl;
    //   // int device_type = get_placement_(GetRef<Expr>(call_node));
    //   // if (device_type > 0) {
    //   //   rc = relay::op::annotation::on_device(post, device_type);
    //   // }
    // }
    return rc;
  }

 private:
  IRModule module_;
};

namespace transform {

Pass ExternalFunctionToInternal() {
  runtime::TypedPackedFunc<Function(Function, IRModule, PassContext)> pass_func =
      [=](Function f, IRModule m, PassContext pc) {
        auto out = Downcast<Function>(FunctionInternalizer(m).Mutate(f));
        std::cout << "-----------------------------------------------" << std::endl;
        std::cout << "ExternalFunctionToInternal" << std::endl;
        // std::cout << "orig" << std::endl;
        // std::cout << AsText(f, false) << std::endl;
        // std::cout << "new" << std::endl;
        // std::cout << AsText(out, false) << std::endl;
        std::cout << "-----------------------------------------------" << std::endl;
        return out;
      };
  return CreateFunctionPass(pass_func, 2, "ExternalFunctionToInternal", {});
}

TVM_REGISTER_GLOBAL("relay._transform.ExternalFunctionToInternal")
    .set_body_typed(ExternalFunctionToInternal);
}  // namespace transform
}  // namespace relay
}  // namespace tvm
