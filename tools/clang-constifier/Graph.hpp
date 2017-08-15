//
// MIT License
//
// Copyright (c) 2017 Jeremy Demeule
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <algorithm>
#include <iosfwd>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace constifier {

struct VisitStatus {
   enum Value { Continue, Stop };
};

template <typename NodeValueT>
class DiGraph;

template <typename It>
class RangeIterator {
public:
   typedef It                         iterator;
   typedef typename It::value_type    Handle_t;
   typedef typename Handle_t::Value_t Value_t;

   RangeIterator(It first, It last)
      : m_first(first)
      , m_last(last) {}

   It begin() const {
      return m_first;
   }

   It end() const {
      return m_last;
   }


   int size() const {
      return std::distance(m_first, m_last);
   }


   bool has(const Value_t& value) const {
      Handle_t tmp(value);
      return has(tmp);
   }

   bool has(const Handle_t& h) const {
      return std::find(m_first, m_last, h) != m_last;
   }

private:
   It m_first;
   It m_last;
};

template <typename It, typename ValueType>
class DerefIterator {
public:
   typedef It base_iterator;

   typedef ValueType  value_type;
   typedef ValueType* pointer;
   typedef ValueType& reference;

   typedef typename std::iterator_traits<base_iterator>::iterator_category iterator_category;
   typedef typename std::iterator_traits<base_iterator>::difference_type   difference_type;


   explicit DerefIterator(base_iterator base)
      : m_base(base) {}

   const value_type& operator*() const {
      return **m_base;
   }

   value_type& operator*() {
      return **m_base;
   }


   const value_type* operator->() const {
      return *m_base;
   }

   value_type* operator->() {
      return *m_base;
   }


   DerefIterator& operator++() {
      ++m_base;
      return *this;
   }

   DerefIterator operator++(int) {
      DerefIterator tmp(*this);
      ++m_base;
      return tmp;
   }


   DerefIterator& operator--() {
      --m_base;
      return *this;
   }

   DerefIterator operator--(int) {
      DerefIterator tmp(*this);
      --m_base;
      return tmp;
   }


   difference_type operator-(const DerefIterator& that) const {
      return m_base - that.m_base;
   }


   bool operator==(const DerefIterator& that) const {
      return m_base == that.m_base;
   }

   bool operator!=(const DerefIterator& that) const {
      return m_base != that.m_base;
   }

private:
   base_iterator m_base;
};


template <typename NodeValueT>
class Node {
public:
   struct NodeLess {
      bool operator()(const Node* a, const Node* b) const {
         return a->value() < b->value();
      }
   };

private:
   typedef std::set<Node*, NodeLess> NodeSet_t;

public:
   typedef NodeValueT Value_t;

   typedef DerefIterator<typename NodeSet_t::const_iterator, Node> NodeIterator_t;
   typedef RangeIterator<NodeIterator_t> Neighbors_t;



   void addNeighbor(Node* n) {
      m_neighbors.insert(n);
   }

   void removeNeighbor(Node* n) {
      m_neighbors.erase(n);
   }

   Neighbors_t Neighbors() const {
      return Neighbors_t(NodeIterator_t(m_neighbors.begin()), NodeIterator_t(m_neighbors.end()));
   }

   Neighbors_t Predecessors() const {
      return Neighbors_t(NodeIterator_t(m_predecessors.begin()), NodeIterator_t(m_predecessors.end()));
   }

   void addPredecessor(Node* n) {
      m_predecessors.insert(n);
   }

   void removePredecessor(Node* n) {
      m_predecessors.erase(n);
   }

   const Value_t& value() const {
      return m_value;
   }

   Value_t& value() {
      return m_value;
   }

private:
   friend class DiGraph<NodeValueT>;

   explicit Node(const Value_t& v)
      : m_value(v)
      , m_neighbors()
      , m_predecessors() {}

private:
   Value_t   m_value;
   NodeSet_t m_neighbors;
   NodeSet_t m_predecessors;  // not used in loaders context
};


template <typename NodeValueT>
class DiGraph {
public:
   typedef NodeValueT       NodeValue_t;
   typedef Node<NodeValueT> Node_t;

   typedef std::set<Node_t*, typename Node_t::NodeLess> NodeSet_t;
   typedef typename NodeSet_t::const_iterator NodeIterator_t;

   typedef std::pair<Node<NodeValueT>*, Node<NodeValueT>*> Edge_t;
   typedef std::vector<Edge_t> Edges_t;

private:
   struct Deleter {
      template <typename T>
      inline void operator()(const T* e) const {
         delete e;
      }
   };

public:
   DiGraph()
      : m_nodes() {}

   DiGraph(const DiGraph& that)
      : m_nodes() {

      for (NodeIterator_t it = that.beginNodes(); it != that.endNodes(); ++it)
         addNode((*it)->value());

      const Edges_t& edges = that.Edges();
      for (typename Edges_t::const_iterator e = edges.begin(); e != edges.end(); ++e)
         addEdge(e->first->value(), e->second->value());
   }

   DiGraph& operator=(const DiGraph& that) {
      DiGraph tmp(that);
      swap(tmp);
      return *this;
   }

   ~DiGraph() {
      clear();
   }

   void swap(DiGraph& that) {
      std::swap(m_nodes, that.m_nodes);
   }

   NodeIterator_t beginNodes() const {
      return m_nodes.begin();
   }

   NodeIterator_t endNodes() const {
      return m_nodes.end();
   }

   Edges_t Edges() const {
      typedef typename Node_t::Neighbors_t   Neighbors_t;
      typedef typename Neighbors_t::iterator NeighborIterator;

      Edges_t edges;

      for (NodeIterator_t it = m_nodes.begin(); it != m_nodes.end(); ++it) {
         Node_t*     n1        = *it;
         Neighbors_t neighbors = n1->Neighbors();
         for (NeighborIterator nh = neighbors.begin(); nh != neighbors.end(); ++nh) {
            Node_t* n2 = &*nh;
            edges.push_back(Edge_t(n1, n2));
         }
      }

      return edges;
   }


   Node_t* addNode(const NodeValue_t& value) {
      Node_t                       key(value);
      typename NodeSet_t::iterator found = m_nodes.find(&key);
      if (found != m_nodes.end())
         return *found;

      Node_t* n = new Node_t(value);
      m_nodes.insert(n);
      return n;
   }

   void delNode(const NodeValue_t& value) {
      Node_t                       key(value);
      typename NodeSet_t::iterator found = m_nodes.find(&key);
      if (found == m_nodes.end())
         return;

      Node_t* n = *found;

      // remove edges
      removeEdges(n);
      // remove from node set
      m_nodes.erase(found);
      // finally delete node
      delete n;
   }

   Node_t* findNode(const NodeValue_t& value) const {
      Node_t                             key(value);
      typename NodeSet_t::const_iterator found = m_nodes.find(&key);
      if (found == m_nodes.end())
         return 0;
      return *found;
   }

   void addEdge(Node_t* n1, Node_t* n2) {
      // assert n1 && n2 are nodes from this graph
      n1->addNeighbor(n2);
      n2->addPredecessor(n1);
   }

   void addEdge(const NodeValue_t& first, const NodeValue_t& second) {
      Node_t* n1 = findNode(first);
      Node_t* n2 = findNode(second);
      if (!n1 || !n2)
         return;
      addEdge(n1, n2);
   }
   // return an Edge? (ie pair of Node_t*)

   void removeEdge(Node_t* n1, Node_t* n2) {
      // assert n1 && n2 are nodes from this graph
      n1->removeNeighbor(n2);
      n2->removePredecessor(n1);
   }

   void removeEdge(const NodeValue_t& first, const NodeValue_t& second) {
      Node_t* n1 = findNode(first);
      Node_t* n2 = findNode(second);
      if (!n1 || !n2)
         return;
      removeEdge(n1, n2);
   }

   void clear() {
      // clear all nodes
      std::for_each(m_nodes.begin(), m_nodes.end(), Deleter());
      m_nodes.clear();
   }

   void print(std::ostream& ostr, const std::string& name = "") const {
      typedef typename Node_t::Neighbors_t       Neighbors_t;
      typedef typename Neighbors_t::iterator     NeighborIterator;
      typedef typename NodeSet_t::const_iterator ConstNodeIterator_t;

      const std::string graphtype = "digraph";
      const std::string link      = "->";

      int id = 0;
      std::map<const Node_t*, int> nodeIds;

      for (ConstNodeIterator_t n = m_nodes.begin(); n != m_nodes.end(); ++n, ++id) {
         nodeIds[*n] = id;
      }

      ostr << graphtype << " \"" << name << "\" {" << std::endl << "  node [shape=box];" << std::endl;

      for (ConstNodeIterator_t n = m_nodes.begin(); n != m_nodes.end(); ++n) {
         std::stringstream buffer;
         buffer << (*n)->value();

         const std::string& value = buffer.str();

         ostr << "  \"" << nodeIds[*n] << "\"";
         if (!value.empty())
            ostr << " [label=\"" << value << "\"]";
         ostr << std::endl;
      }

      const Edges_t& edges = Edges();
      for (typename Edges_t::const_iterator e = edges.begin(); e != edges.end(); ++e) {
         ostr << "  \"" << nodeIds[e->first] << "\" " << link << " \"" << nodeIds[e->second] << "\"" << std::endl;
      }
      ostr << "}" << std::endl;
   }

private:
   void removeEdges(Node_t* n) {
      typedef typename Node_t::Neighbors_t   Neighbors_t;
      typedef typename Neighbors_t::iterator NeighborIterator;

      // for each predecessors, remove n from neighbors
      Neighbors_t predecessors = n->Predecessors();
      for (NeighborIterator it = predecessors.begin(), last = predecessors.end(); it != last; ++it) {
         it->removeNeighbor(n);
      }

      // for each neighbors, remove n from predessors
      Neighbors_t neighbors = n->Neighbors();
      for (NeighborIterator it = neighbors.begin(), last = neighbors.end(); it != last; ++it) {
         it->removePredecessor(n);
      }
   }


private:
   NodeSet_t m_nodes;
};

template <typename NodeValueT>
inline std::ostream& operator<<(std::ostream& ostr, const DiGraph<NodeValueT>& g) {
   g.print(ostr);
   return ostr;
};

template <typename G>
struct NamedGraph {

   NamedGraph(const G& graph, std::string name)
      : g(graph)
      , graphname(name) {}

   const G&    g;
   std::string graphname;
};

template <typename G>
NamedGraph<G> make_named_graph(const G& g, std::string name) {
   return NamedGraph<G>(g, name);
}

template <typename G>
inline std::ostream& operator<<(std::ostream& ostr, const NamedGraph<G>& namedgraph) {
   namedgraph.g.print(ostr, namedgraph.graphname);
   return ostr;
};


// Algorihtms



namespace impl {

template <typename G, typename VisitorHandler>
VisitStatus::Value DFSVisit_impl(const G&                             g,
                                 const typename G::Node_t*            n,
                                 VisitorHandler&                      handler,
                                 std::set<const typename G::Node_t*>& visited) {

   if (visited.find(n) != visited.end())
      return VisitStatus::Continue;

   visited.insert(n);

   if (handler.examineNode(*n) == VisitStatus::Stop)
      return VisitStatus::Stop;

   typedef typename G::Node_t   Node_t;
   typename Node_t::Neighbors_t neighbors = n->Neighbors();
   for (typename Node_t::NodeIterator_t nn = neighbors.begin(); nn != neighbors.end(); ++nn) {
      if (DFSVisit_impl(g, &*nn, handler, visited) == VisitStatus::Stop)
         return VisitStatus::Stop;
   }

   if (handler.finishNode(*n) == VisitStatus::Stop)
      return VisitStatus::Stop;

   return VisitStatus::Continue;
}

template <typename G, typename Handler>
struct TopologicalHandler {

   TopologicalHandler()                          = default;
   TopologicalHandler(const TopologicalHandler&) = default;
   TopologicalHandler& operator=(const TopologicalHandler&) = default;

   explicit TopologicalHandler(Handler& h)
      : handler(h) {}

   VisitStatus::Value examineNode(const typename G::Node_t&) {
      return VisitStatus::Continue;
   }
   VisitStatus::Value finishNode(const typename G::Node_t& n) {
      handler(n);
      return VisitStatus::Continue;
   }

   Handler& handler;
};
}


template <typename G, typename VisitorHandler>
void DFSVisit(const G& g, const typename G::Node_t* src, VisitorHandler handler) {
   std::set<const typename G::Node_t*> visited;
   impl::DFSVisit_impl(g, src, handler, visited);
}

template <typename G, typename VisitorHandler>
void DFSVisit(const G& g, VisitorHandler handler) {

   std::set<const typename G::Node_t*> visited;

   for (typename G::NodeIterator_t n = g.beginNodes(); n != g.endNodes(); ++n) {
      if (impl::DFSVisit_impl(g, *n, handler, visited) == VisitStatus::Stop)
         return;
   }
}


template <typename G, typename Handler>
void TopologicalVisit(const G& g, Handler h) {
   DFSVisit(g, impl::TopologicalHandler<G, Handler>(h));
}


template <typename G, typename VisitorHandler>
class BFSVisitor {
public:
   typedef typename G::Node_t Node_t;

   VisitStatus::Value visit(const G& g, const Node_t* src, VisitorHandler handler) {
      if (!src)
         return VisitStatus::Continue;

      if (visited.find(src) != visited.end())
         return VisitStatus::Continue;

      q.push(src);

      while (!q.empty()) {
         const Node_t* n = q.front();
         q.pop();

         visited.insert(n);  // mark

         if (handler.examineNode(*n) == VisitStatus::Stop)
            return VisitStatus::Stop;

         typename Node_t::Neighbors_t neighbors = n->Neighbors();
         for (typename Node_t::NodeIterator_t nn = neighbors.begin(); nn != neighbors.end(); ++nn) {

            if (handler.examineEdge(*n, *nn) == VisitStatus::Stop)
               return VisitStatus::Stop;

            if (visited.find(&(*nn)) == visited.end())  // is marked?
               q.push(&(*nn));
         }

         if (handler.finishNode(*n) == VisitStatus::Stop)
            return VisitStatus::Stop;
      }
      return VisitStatus::Continue;
   }

private:
   std::queue<const Node_t*> q;
   std::set<const Node_t*>   visited;
};


template <typename G, typename VisitorHandler>
void BFSVisit(const G& g, const typename G::Node_t* src, VisitorHandler handler) {
   BFSVisitor<G, VisitorHandler> bfs;
   bfs.visit(g, src, handler);
}

template <typename G, typename VisitorHandler>
void BFSVisit(const G& g, VisitorHandler handler) {
   BFSVisitor<G, VisitorHandler> bfs;

   for (typename G::NodeIterator_t n = g.beginNodes(); n != g.endNodes(); ++n) {
      if (bfs.visit(g, *n, handler) == VisitStatus::Stop)
         return;
   }
}
}

#endif
