// Copyright 2017-2021 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/formatting/tree_reshaper.h"

#include "common/formatting/line_wrap_searcher.h"

namespace verible {

std::ostream& operator<<(std::ostream& stream, const TreeReshaper::Layout& layout) {
  switch (layout.type_) {
    case TreeReshaper::LayoutType::kLine:
      return stream << "[<horizontal>]";
    case TreeReshaper::LayoutType::kStack:
      return stream << "[<vertical>]";
    case TreeReshaper::LayoutType::kText:
      return stream << layout.uwline_;
    case TreeReshaper::LayoutType::kWrap:
      return stream << "[<wrap>]";
    case TreeReshaper::LayoutType::kChoice:
      return stream << "[<choice>]";
  }
  LOG(FATAL) << "Unknown TreeReshaper::Layout type " << int(layout.type_);
}

TreeReshaper::KnotSet* TreeReshaper::HPlusSolution(
    const TreeReshaper::KnotSet& left, const TreeReshaper::KnotSet& right,
    const BasicFormatStyle& style) {
  KnotSet* ret = new KnotSet{};

  auto s1 = left.begin();
  auto s2 = right.begin();

  auto s1_margin = 0;
  auto s2_margin = s1->span_ + s2->before_spaces_;
  s2.MoveToMargin(s2_margin);

  while (true) {
    auto g1 = s1->gradient_;
    auto g2 = s2->gradient_;

    auto overhang = s2_margin - style.column_limit;
    auto g_cur = g1 + g2 - style.over_column_limit_penalty * (overhang >= 0);
    auto i_cur = s1.ValueAt(s1_margin) + s2.ValueAt(s2_margin) -
        style.over_column_limit_penalty * std::max(overhang, 0);

    ret->AppendKnot(Knot(
        s1_margin, s1->span_ + s2->span_ + s2->before_spaces_, i_cur, g_cur,
        new LayoutTree{
            LayoutType::kLine,
            *ABSL_DIE_IF_NULL(s1->layout_),
            *ABSL_DIE_IF_NULL(s2->layout_)}, s1->before_spaces_));

    auto kn1 = s1.NextKnot();
    auto kn2 = s2.NextKnot();

    const auto inf = std::numeric_limits<int>::max();
    if (kn1 == inf && kn2 == inf) {
      break;
    }

    if (kn1 - s1_margin <= kn2 - s2_margin) {
      ++s1;
      s1_margin = kn1;
      s2_margin = s1_margin + s1->span_ + s2->before_spaces_;
      s2.MoveToMargin(s2_margin);
    } else {
      ++s2;
      s2_margin = kn2;
      s1_margin = s2_margin - s1->span_ - s2->before_spaces_;
    }
  }

  return ret;
}

TreeReshaper::KnotSet* TreeReshaper::VSumSolution(
    const TreeReshaper::SolutionSet& solution_set, const BasicFormatStyle& style) {
  KnotSet* ret = new KnotSet{};

  int margin = 0;

  std::vector<TreeReshaper::KnotSet::iterator> set;
  for (auto& iter : solution_set) {
    set.push_back(iter->begin());
  }

  while (true) {
    int i_cur = 0;
    int g_cur = 0;
    auto* l_cur = new LayoutTree{LayoutType::kStack};

    for (auto& itr : set) {
      i_cur += itr.ValueAt(margin);
      g_cur += itr->gradient_;
      l_cur->AdoptSubtree(*ABSL_DIE_IF_NULL(itr->layout_));
    }

    ret->AppendKnot(Knot(margin, set.back()->span_,
                         i_cur, g_cur, l_cur, set.front()->before_spaces_));

    const auto inf = std::numeric_limits<int>::max();
    int d_star = inf;

    for (auto& itr : set) {
      int knot = itr.NextKnot();

      if (knot == inf || knot <= margin) {
        continue ;
      }

      if ((knot - margin) < d_star) {
        d_star = knot - margin;
      }
    }

    if (d_star == inf) {
      break;
    }

    margin += d_star;
    for (auto& itr : set) {
      itr.MoveToMargin(margin);
    }
  }

  return ret;
}

TreeReshaper::KnotSet* TreeReshaper::MinSolution(
    const TreeReshaper::SolutionSet& solution_set, const BasicFormatStyle& style) {
  if (solution_set.size() == 0) {
    return new KnotSet{};
  } else if (solution_set.size() == 1) {
    return solution_set[0]->Clone();
  }

  KnotSet* ret = new KnotSet{};

  std::vector<TreeReshaper::KnotSet::iterator> set;
  for (auto& iter : solution_set) {
    set.push_back(iter->begin());
  }

  int k_l = 0;
  int last_i_min_soln = -1;
  int last_index = -1;

  const auto inf = std::numeric_limits<int>::max();

  // FIXME(ldk): Make sure this isn't an infinite loop ...
  while (k_l < inf) {
    int k_h = std::min_element(set.begin(), set.end(),
        [](const TreeReshaper::KnotSet::iterator& a,
           const TreeReshaper::KnotSet::iterator& b) {
          return a.NextKnot() < b.NextKnot();
        })->NextKnot() - 1;

    std::vector<int> gradients;
    std::transform(set.begin(), set.end(), std::back_inserter(gradients),
        [](const TreeReshaper::KnotSet::iterator& s) -> int {
          return s->gradient_;
        });

    // FIXME(ldk): ... nor this loop is
    bool kg = true;
    while (kg) {
      std::vector<int> values;
      std::transform(set.begin(), set.end(), std::back_inserter(values),
          [&k_l](const TreeReshaper::KnotSet::iterator& s) -> int {
            return s.ValueAt(k_l);
          });

      auto min_value_itr = std::min_element(values.begin(), values.end());
      auto i_min_sol = std::distance(values.begin(), min_value_itr);
      auto min_value = *min_value_itr;
      auto min_gradient = gradients[i_min_sol];
      auto& min_soln = set[i_min_sol];
      auto min_soln_idx = std::distance(solution_set[i_min_sol]->begin(), min_soln);

      if ((i_min_sol != last_i_min_soln) || (min_soln_idx != last_index)) {
        ret->AppendKnot(Knot(k_l,
            min_soln->span_, min_value, min_gradient, min_soln->layout_,
            min_soln->before_spaces_));
        last_i_min_soln = i_min_sol;
        last_index = min_soln_idx;
      }

      std::vector<int> distances_to_cross;
      for (unsigned int i = 0 ; i < set.size() ; i += 1) {
        if (gradients[i] >= min_gradient) {
          continue ;
        }

        double gamma = (values[i] - min_value) / (min_gradient - gradients[i]);
        distances_to_cross.push_back(ceil(gamma));
      }

      std::vector<int> crossovers;
      for (const auto& d : distances_to_cross) {
        if (d > 0 && k_l + d <= k_h) {
          crossovers.push_back(k_l + d);
        }
      }

      if (crossovers.size() > 0) {
        k_l = *std::min_element(crossovers.begin(), crossovers.end());
      } else {
        k_l = k_h + 1;
        if (k_l < inf) {
          std::for_each(set.begin(), set.end(),
              [&k_l](TreeReshaper::KnotSet::iterator& s) { s.MoveToMargin(k_l); });
        }
        kg = false; // break parent loop
        break;
      }
    }
  }

  return ret;
}

TreeReshaper::KnotSet* TreeReshaper::ComputeSolution(
    const TreeReshaper::BlockTree& tree,
    const TreeReshaper::KnotSet& rest_of_line,
    const BasicFormatStyle& style) {
  const auto type = tree.Value().type_;
  //const auto& children = tree.Children();

  switch (type) {
    case TreeReshaper::LayoutType::kText: {
      if (rest_of_line.size() == 0) {
        TreeReshaper::KnotSet* ret = new TreeReshaper::KnotSet{};

        // FIXME(ldk): Handle lines with MustWrap tokens
        const auto& uwline = tree.Value().uwline_;
        int span = UnwrappedLineLength(uwline, style);
        CHECK_GE(span, 0);

        if (span >= style.column_limit) {
          ret->AppendKnot(TreeReshaper::Knot(
              0, span, (span - style.column_limit) *
              style.over_column_limit_penalty,
              style.over_column_limit_penalty,
              new TreeReshaper::LayoutTree{uwline}));
        } else {
          ret->AppendKnot(TreeReshaper::Knot(
              0, span, 0, 0,
              new TreeReshaper::LayoutTree{uwline}));
          ret->AppendKnot(TreeReshaper::Knot(
              style.column_limit - span, span, 0,
              style.over_column_limit_penalty,
              new TreeReshaper::LayoutTree{uwline}));
        }

        return ret;
      } else {
        return ComputeSolution(tree,
            TreeReshaper::KnotSet{}, style)->WithRestOfLine(rest_of_line, style);
      }
    }

    case TreeReshaper::LayoutType::kStack: {
      TreeReshaper::SolutionSet slns;

      for (auto itr = tree.Children().begin() ;
          itr != std::prev(tree.Children().end()) ; ++itr) {
        slns.push_back(TreeReshaper::ComputeSolution(*itr,
            TreeReshaper::KnotSet{}, style));
      }
      slns.push_back(TreeReshaper::ComputeSolution(
          tree.Children().back(), rest_of_line, style));

      TreeReshaper::KnotSet* set = TreeReshaper::VSumSolution(slns, style);

      int plus = 0;
      if (tree.Children().size() > 1) {
        plus = tree.Children().size() - 1;
      }

      return set->PlusConst(plus * style.line_break_penalty);
    }

    case TreeReshaper::LayoutType::kLine: {
      KnotSet* set = rest_of_line.Clone();

      for (auto itr = tree.Children().rbegin() ;
          itr != tree.Children().rend() ; ++itr) {
        // FIXME(ldk): Memory leak
        set = TreeReshaper::ComputeSolution(*itr, *set, style);
      }

      return set;
    }

    case TreeReshaper::LayoutType::kChoice: {
      TreeReshaper::SolutionSet slns;

      if (tree.Children().size() == 0) {
        return new TreeReshaper::KnotSet{};
      }

      for (unsigned i = 0 ; i < tree.Children().size() - 1 ; ++i) {
        slns.push_back(
            TreeReshaper::ComputeSolution(tree.Children()[i],
                                          TreeReshaper::KnotSet{}, style));
      }

      slns.push_back(TreeReshaper::ComputeSolution(
          tree.Children().back(), rest_of_line, style));

      return TreeReshaper::MinSolution(slns, style);
    }

    case TreeReshaper::LayoutType::kWrap: {
      TreeReshaper::MutableSolutionSet elt_layouts;

      if (tree.Children().size() == 0) {
        return new TreeReshaper::KnotSet{};
      }

      for (const auto& itr : tree.Children()) {
        elt_layouts.push_back(TreeReshaper::ComputeSolution(
            itr, TreeReshaper::KnotSet{}, style));
      }

      TreeReshaper::MutableSolutionSet wrap_solutions;
      for (unsigned i = 0 ; i < tree.Children().size() ; ++i) {
        wrap_solutions.push_back(new TreeReshaper::KnotSet{});
      }

      const int n = tree.Children().size();
      for (int i = n - 1 ; i >= 0 ; --i) {
        TreeReshaper::SolutionSet solution_i;
        KnotSet* line_layout = elt_layouts[i];

        for (int j = i ; j < n - 1 ; ++j) {
          TreeReshaper::SolutionSet tmp_i;
          tmp_i.push_back(line_layout);
          tmp_i.push_back(wrap_solutions[j + 1]);
          auto* full_soln = TreeReshaper::VSumSolution(tmp_i, style);

          // FIXME(ldk): cpack?
          const double cpack = 1e-3;  // FIXME(ldk): Try other values, e.g. 0.3, 0.7
          solution_i.push_back(full_soln->PlusConst(
              style.line_break_penalty + cpack * (n - j)));
          line_layout = line_layout->WithRestOfLine(
              *ABSL_DIE_IF_NULL(elt_layouts[j + 1]), style);
        }

        solution_i.push_back(line_layout->WithRestOfLine(rest_of_line, style));
        wrap_solutions[i] = TreeReshaper::MinSolution(solution_i, style);
      }

      return wrap_solutions.size()>0?wrap_solutions[0]:new TreeReshaper::KnotSet{};
    }

    default: {
      //return new TreeReshaper::KnotSet{};
      return nullptr;
    }
  }
}

TokenPartitionTree* TreeReshaper::BuildTokenPartitionTree(const LayoutTree& layout) {
  const auto type = layout.Value().type_;

  switch (type) {
    case TreeReshaper::LayoutType::kText: {
      // FIXME(ldk): Use accessor instead of class field
      return new TokenPartitionTree(layout.Value().uwline_);
    }

    case TreeReshaper::LayoutType::kStack: {
      // FIXME(ldk): Not sure about this, should such thing happen?
      if (layout.Children().size() == 0) {
        return new TokenPartitionTree(UnwrappedLine());
      } else if (layout.Children().size() == 1) {
        return ABSL_DIE_IF_NULL(BuildTokenPartitionTree(layout.Children()[0]));
      }

      TokenPartitionTree* tree = nullptr;

      for (const auto& itr : layout.Children()) {
        auto* layout = ABSL_DIE_IF_NULL(BuildTokenPartitionTree(itr));
        if (tree == nullptr) {
          tree = new TokenPartitionTree(layout->Value());
          tree->Value().SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
        }

        if (layout->Children().size() == 0) {
          tree->Value().SpanUpToToken(layout->Value().TokensRange().end());
          tree->AdoptSubtree(*layout);
        } else {
          for (auto& i : layout->Children()) {
            CHECK_EQ(i.Children().size(), 0);
            tree->Value().SpanUpToToken(i.Value().TokensRange().end());
            tree->AdoptSubtree(i);
          }
        }
      }

      return tree;
    }

    case TreeReshaper::LayoutType::kLine: {
      CHECK_EQ(layout.Children().size(), 2);

      const auto& layout_1 = layout.Children()[0];
      const auto& layout_2 = layout.Children()[1];

      auto* tree_1 = ABSL_DIE_IF_NULL(BuildTokenPartitionTree(layout_1));
      auto* tree_2 = ABSL_DIE_IF_NULL(BuildTokenPartitionTree(layout_2));

      if (tree_1->Children().size() == 0 && tree_2->Children().size() == 0) {
        UnwrappedLine uwline = tree_1->Value();
        uwline.SpanUpToToken(tree_2->Value().TokensRange().end());
        uwline.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);

        return new TokenPartitionTree(uwline);
      } else if (tree_1->Children().size() == 0 && tree_2->Children().size() >= 2) {
        VLOG(4) << "horizontal merge (0, >=2):\n" <<
            verible::TokenPartitionTreePrinter(*tree_1) << "\ntree_2:\n" <<
            verible::TokenPartitionTreePrinter(*tree_2);

        BasicFormatStyle style;  // FIXME(ldk): As parameter
        const auto indent = UnwrappedLineLength(tree_1->Value(), style);
        const auto self_indent = tree_1->Value().IndentationSpaces();

        int extra_spaces = 0;
        {  // FIXME(ldk): extra space? btw. This shouldn't be here
          UnwrappedLine euw = UnwrappedLine(tree_1->Value());
          euw.SpanUpToToken(tree_2->Children()[0].Value().TokensRange().end());
          extra_spaces += (UnwrappedLineLength(euw, style) - indent -
                               UnwrappedLineLength(tree_2->Children()[0].Value(), style))>0?1:0;
        }

        UnwrappedLine uwline = tree_1->Value();
        uwline.SpanUpToToken(tree_2->Value().TokensRange().end());
        uwline.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
        auto* tree = new TokenPartitionTree(uwline);

        {
          UnwrappedLine uw = tree_1->Value();
          uw.SpanUpToToken(tree_2->Children()[0].Value().TokensRange().end());
          uw.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);

          auto* subtree = new TokenPartitionTree(uw);

          tree_2->Children().erase(tree_2->Children().begin());
          tree_2->Value().SpanBackToToken(
              tree_2->Children()[0].Value().TokensRange().begin());

          tree->AdoptSubtree(*subtree);
        }

        tree_2->ApplyPreOrder([&indent,&self_indent,&extra_spaces](TokenPartitionTree& node) {
              node.Value().SetIndentationSpaces(
                  node.Value().IndentationSpaces() + indent - self_indent + extra_spaces);
            });

        tree->AdoptSubtreesFrom(tree_2);

        VLOG(4) << "merged:\n" << verible::TokenPartitionTreePrinter(*tree);
        return tree;
      } else if (tree_1->Children().size() >= 2 && tree_2->Children().size() == 0) {
        //VLOG(4) << "horizontal merge [stack, line]:\n" <<
        //    verible::TokenPartitionTreePrinter(*tree_1) << "\ntree_2:\n" <<
        //    verible::TokenPartitionTreePrinter(*tree_2);

        tree_1->Value().SpanUpToToken(
            tree_2->Value().TokensRange().end());

        // Append tree_2 tokens to last children
        tree_1->Children().back().Value().SpanUpToToken(
            tree_2->Value().TokensRange().end());

        //VLOG(4) << "merged:\n" << verible::TokenPartitionTreePrinter(*tree_1);
        return tree_1;
      }

      LOG(FATAL) << " *** Unsupported configuration: " <<
          tree_1->Children().size() << ", " <<
          tree_2->Children().size() << "\n" <<
          verible::TokenPartitionTreePrinter(*tree_1) << "\n" <<
          verible::TokenPartitionTreePrinter(*tree_2);

      return nullptr;
    }

    case TreeReshaper::LayoutType::kChoice:
    case TreeReshaper::LayoutType::kWrap: {
      // Should not happen
      LOG(FATAL) << "Invalid layout type: " << int(type);
    }
  }

  // Should not happen
  LOG(FATAL) << "Unknown layout: " << int(type);
  return nullptr;
}


TreeReshaper::LayoutTree* TreeReshaper::BuildLayoutTreeFromTokenPartitionTree(
    const TokenPartitionTree& token_partition_tree) {
  TreeReshaper::BlockTree* dynamic_layout_tree =
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText};

  BasicFormatStyle style;  // FIXME: by argument

  *dynamic_layout_tree = token_partition_tree.Transform<TreeReshaper::Layout>(
      [](const TokenPartitionTree& node) -> TreeReshaper::Layout {
        TreeReshaper::Layout layout{TreeReshaper::LayoutType::kText};
        layout.uwline_ = node.Value();

        if (node.Children().empty()) {
          layout.type_ = TreeReshaper::LayoutType::kText;
        } else {
          const auto policy = node.Value().PartitionPolicy();
          switch (policy) {
            case PartitionPolicyEnum::kApplyOptimalLayout: {
              layout.type_ = TreeReshaper::LayoutType::kChoice;
              break;
            }
            case PartitionPolicyEnum::kWrapSubPartitions: {
              layout.type_ = TreeReshaper::LayoutType::kWrap;
              break;
            }
            default: {
              layout.type_ = TreeReshaper::LayoutType::kText;
              break;
            }
          }
        }

        return layout;
      });


  // Split kText layout if neccessary
  dynamic_layout_tree->ApplyPostOrder(
      [&style](TreeReshaper::LayoutTree& node) {
        const auto type = node.Value().type_;

        if (type != TreeReshaper::LayoutType::kText) {
          return ;
        }

        const auto uwline = node.Value().uwline_;

        std::vector<UnwrappedLine*> sublines;

        const auto length = UnwrappedLineLength(uwline, style);
        if (length < 0) {
          sublines.push_back(new UnwrappedLine(0, uwline.TokensRange().begin()));
          for (auto itr = uwline.TokensRange().begin();
              itr != uwline.TokensRange().end(); ++itr) {
            if (itr->before.break_decision == SpacingOptions::MustWrap) {
              sublines.back()->SpanUpToToken(itr);
              sublines.push_back(new UnwrappedLine(0, itr));
            }
            sublines.back()->SpanUpToToken(uwline.TokensRange().end());
          }

          node.Value().type_ = TreeReshaper::LayoutType::kStack;
          for (const auto* itr : sublines) {
            node.AdoptSubtree(LayoutTree{Layout{*itr}});
          }
        }
      });

  VLOG(4) << "\n" << *dynamic_layout_tree;

  dynamic_layout_tree->ApplyPostOrder(
      [](TreeReshaper::LayoutTree& node) {
        const auto type = node.Value().type_;

        if (type == TreeReshaper::LayoutType::kChoice) {
          // Choice between line & stack layout
          auto line_tree = node.Transform<TreeReshaper::Layout>(
              [](const TreeReshaper::LayoutTree& node) -> TreeReshaper::Layout {
                return node.Value();
              });

          auto stack_tree = node.Transform<TreeReshaper::Layout>(
              [](const TreeReshaper::LayoutTree& node) -> TreeReshaper::Layout {
                return node.Value();
              });

          const BasicFormatStyle style;
          stack_tree.Children()[1].ApplyPreOrder(
              [&style](TreeReshaper::LayoutTree& node) {
                auto uwline = node.Value().uwline_;
                uwline.SetIndentationSpaces(
                    uwline.IndentationSpaces() + style.wrap_spaces);

                const auto type = node.Value().type_;
                node.Value().uwline_ = uwline;
                node.Value().type_ = type;
              });

          line_tree.Value().type_ = TreeReshaper::LayoutType::kLine;
          stack_tree.Value().type_ = TreeReshaper::LayoutType::kStack;

          node.Children().clear();
          node.AdoptSubtree(line_tree);
          node.AdoptSubtree(stack_tree);
        }
      });

  VLOG(4) << "post xform:\n" << *dynamic_layout_tree;
  return dynamic_layout_tree;
}

void TreeReshaper::ReshapeTokenPartitionTree(TokenPartitionTree* tree,
                                             const BasicFormatStyle style) {
  //LOG(INFO) << "tree:\n" << verible::TokenPartitionTree(*tree) << "\n";
  const auto indent = tree->Value().IndentationSpaces();
  tree->ApplyPreOrder([](UnwrappedLine& line) {
      line.SetIndentationSpaces(0);
    });
  VLOG(3) << "indent: " << indent << ", tree:\n" <<
      verible::TokenPartitionTree(*tree) << "\n";

  TreeReshaper::LayoutTree* layout_tree =
      TreeReshaper::BuildLayoutTreeFromTokenPartitionTree(*ABSL_DIE_IF_NULL(tree));

  BasicFormatStyle s = style;
  s.column_limit -= indent;

  const auto* sut_ptr = TreeReshaper::ComputeSolution(
      *ABSL_DIE_IF_NULL(layout_tree), TreeReshaper::KnotSet{}, s);
  const auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
  VLOG(3) << "solution:\n" << *ABSL_DIE_IF_NULL(sut[0].layout_);
  TokenPartitionTree* reshaped_tree =
      TreeReshaper::BuildTokenPartitionTree(*ABSL_DIE_IF_NULL(sut[0].layout_));
  VLOG(3) << "reshaped_tree:\n" << verible::TokenPartitionTree(*reshaped_tree) << "\n";
  const auto policy = reshaped_tree->Value().PartitionPolicy();
  tree->Children().clear();
  tree->AdoptSubtreesFrom(reshaped_tree);
  tree->ApplyPreOrder([&indent](UnwrappedLine& line) {
      line.SetIndentationSpaces(line.IndentationSpaces()+indent);
    });
  tree->Value().SetPartitionPolicy(policy);
  VLOG(3) << "adopted_tree:\n" << verible::TokenPartitionTree(*tree) << "\n";
}

}  // namespace verible
