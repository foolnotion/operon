#include <algorithm>
#include <execution>

#include "core/grammar.hpp"
#include "core/dataset.hpp"
#include "operators/initialization.hpp"

namespace Operon
{
    Node SampleProportional(operon::rand_t& random, const std::vector<std::pair<NodeType, double>>& partials) 
    {
        std::uniform_real_distribution<double> uniformReal(0, partials.back().second - std::numeric_limits<double>::epsilon());
        auto r    = uniformReal(random);
        auto it   = find_if(partials.begin(), partials.end(), [=](auto& p) { return p.second > r; });
        auto node = Node(it->first);
        return node;
    }

    void Grow(operon::rand_t& random, const Grammar& grammar, const gsl::span<const Variable> variables, std::vector<Node>& nodes, const std::vector<std::pair<NodeType, double>>& partials, size_t maxBranchLength, size_t maxBranchDepth, size_t minFunctionArity) 
    {
        if (maxBranchDepth == 0 || maxBranchLength == 1 || maxBranchLength <= minFunctionArity)
        {
            // only allowed to grow leaf nodes
            auto pc   = grammar.GetFrequency(NodeType::Constant);
            auto pv   = grammar.GetFrequency(NodeType::Variable);
            std::uniform_real_distribution<double> uniformReal(0, pc + pv);
            auto node = uniformReal(random) < pc ? Node(NodeType::Constant) : Node(NodeType::Variable);
            nodes.push_back(node);
        }
        else 
        {
            std::vector<std::pair<NodeType, double>> candidates;
            std::copy_if(partials.begin(), partials.end(), back_inserter(candidates), [&](auto& p) { 
                if (p.first > NodeType::Square) { return false; }
                auto minLength = p.first < NodeType::Log ? 2U : 1U;
                return minLength < maxBranchLength;
            });
            if (candidates.empty())
            {
                throw std::runtime_error(fmt::format("Could not grow tree branch that would satisfy a max branch length of {} (min length = {})\n.", maxBranchLength, minFunctionArity + 1));
            }

            auto node = SampleProportional(random, candidates);
            nodes.push_back(node);
            for (size_t i = 0; i < node.Arity; ++i)
            {
                auto maxChildLength = (maxBranchLength - 1) / node.Arity;
                Grow(random, grammar, variables, nodes, partials, maxChildLength, maxBranchDepth - 1, minFunctionArity);
            }
        }
    }

    Tree GrowTreeCreator::operator()(operon::rand_t& random, const Grammar& grammar, const gsl::span<const Variable> variables) const
    {
        auto allowed = grammar.AllowedSymbols();
        std::vector<std::pair<NodeType, double>> partials(allowed.size());
        std::inclusive_scan(std::execution::seq, allowed.begin(), allowed.end(), partials.begin(), [](const auto& lhs, const auto& rhs) { return std::make_pair(rhs.first, lhs.second + rhs.second); });
        std::vector<Node> nodes;
        auto root = SampleProportional(random, partials);
        nodes.push_back(root);

        auto minFunctionArity = grammar.MinimumFunctionArity();

        for (int i = 0; i < root.Arity; ++i)
        {
            auto maxBranchLength = (maxLength - 1) / root.Arity;
            auto maxBranchDepth  = maxDepth - 1;
            Grow(random, grammar, variables, nodes, partials, maxBranchLength, maxBranchDepth, minFunctionArity);
        }
        std::uniform_int_distribution<size_t> uniformInt(0, variables.size() - 1); 
        std::normal_distribution<double> normalReal(0, 1);
        for(auto& node : nodes)
        {
            if (node.IsVariable())
            {
                node.HashValue = node.CalculatedHashValue = variables[uniformInt(random)].Hash;
            }
            if (node.IsLeaf())
            {
                node.Value = normalReal(random);
            }
        }
        reverse(nodes.begin(), nodes.end());
        auto tree = Tree(nodes);
        tree.UpdateNodes();
        if (tree.Length() > maxLength)
        {
            throw std::runtime_error(fmt::format("Tree length {} exceeds maximum length of {}\n", tree.Length(), maxLength));
        }
        return tree;
    }
}
