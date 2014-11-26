#include "config.h"

#include "context.hpp"
#include "my_assert.h"

#include <cstring>
#include <cstdio>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

namespace trace {
  typedef std::chrono::nanoseconds ns;

  int (*nodeIndexFunction)() = NULL;

  /** \brief Set the function used to get the root index.

      \return a nuber between 0 (not in a parallel region) and the number of
      workers (included).
   */
  static int currentNodeIndex() {
    return (nodeIndexFunction ? nodeIndexFunction() : -1) + 1;
  }

  void setNodeIndexFunction(int (*nodeIndexFunc)()) {
    nodeIndexFunction = nodeIndexFunc;
  }

  bool Node::enabled = true;
  std::unordered_map<void*, Node*> Node::currentNodes[MAX_ROOTS];
  void* Node::enclosingContext[MAX_ROOTS] = {};

  Node::Node(const char* _name, Node* _parent)
    : name(_name), data(), parent(_parent), children() {}

  Node::~Node() {
    for (auto it = children.begin(); it != children.end(); ++it) {
      delete *it;
    }
  }

  void Node::enterContext(const char* name) {
    Node* current = currentNode();
    myAssert(current);
    Node* child = current->findChild(name);
    int index = currentNodeIndex();
    void* enclosing = enclosingContext[index];

    if (!child) {
      child = new Node(name, current);
      current->children.push_back(child);
    }
    myAssert(child);
    currentNodes[index][enclosing] = child;
    current = child;
    current->data.lastEnterTime = std::chrono::high_resolution_clock::now();
    current->data.n += 1;
  }

  void Node::leaveContext() {
    int index = currentNodeIndex();
    void* enclosing = enclosingContext[index];
    Node* current = currentNodes[index][enclosing];
    myAssert(current);

    Time now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<ns>(now - current->data.lastEnterTime);
    current->data.totalTime += duration.count();

    if (!(current->parent)) {
      std::cout << "Warning! Closing root node." << std::endl;
    } else {
      currentNodes[index][enclosing] = current->parent;
    }
  }

  void* Node::currentReference() {
    return (void*) Node::currentNode();
  }

  void Node::setEnclosingContext(void* enclosing) {
    int index = currentNodeIndex();
    enclosingContext[index] = enclosing;
  }

  void Node::incrementFlops(int64_t flops) {
    currentNode()->data.totalFlops += flops;
  }

  void Node::startComm() {
    currentNode()->data.lastCommInitiationTime =
      std::chrono::high_resolution_clock::now();
  }

  void Node::endComm() {
    Node* current = currentNode();
    Time now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<ns>(now - current->data.lastEnterTime);
    current->data.totalCommTime += duration.count();
  }

  Node* Node::findChild(const char* name) const {
    for (auto it = children.begin(); it != children.end(); ++it) {
      // On cherche la correspondance avec le pointeur. Puisqu'on demande que
      // tous les noms soient des pointeurs qui existent tout le long de
      // l'execution, on peut forcer l'unicite.
      if ((*it)->name == name) {
        return *it;
      }
    }
    return NULL;
  }

  void Node::dump(std::ofstream& f) const {
    f << "{"
      << "\"name\": \"" << name << "\", "
      << "\"id\": \"" << this << "\", "
      << "\"n\": " << data.n << ", "
      << "\"totalTime\": " << data.totalTime / 1e9 << ", "
      << "\"totalFlops\": " << data.totalFlops << ", "
      << "\"totalBytesSent\": " << data.totalBytesSent << ", "
      << "\"totalBytesReceived\": " << data.totalBytesReceived << ", "
      << "\"totalBytesReceived\": " << data.totalBytesReceived << ", "
      << "\"totalCommTime\": " << data.totalCommTime / 1e9 << "," << std::endl;
    f << "\"children\": [";
    std::string delimiter("");
    for (auto it = children.begin(); it != children.end(); ++it) {
      f << delimiter;
      (*it)->dump(f);
      delimiter = ", ";
    }
    f << "]}";
  }

  void Node::jsonDump(const char* filename) {
    std::ofstream f(filename);

    f << "[";
    std::string delimiter("");
    for (int i = 0; i < MAX_ROOTS; i++) {
      if (!currentNodes[i].empty()) {
        for (auto p : currentNodes[i]) {
          Node* root = p.second;
          f << delimiter << std::endl;
          root->dump(f);
          delimiter = ", ";
        }
      }
    }
    f << std::endl << "]" << std::endl;
  }

  /** Find the current node, allocating one if necessary.
   */
  Node* Node::currentNode() {
    int id = currentNodeIndex();
    void* enclosing = enclosingContext[id];
    auto it = currentNodes[id].find(enclosing);
    Node* current;
    if (it == currentNodes[id].end()) {
      char *name = const_cast<char*>("root");
      if (id != 0) {
        name = strdup("Worker #XXX - 0xXXXXXXXXXXXXXXXX"); // Worker ID - enclosing
        strongAssert(name);
        sprintf(name, "Worker #%03d - %p", id, enclosing);
      }
      current = new Node(name, NULL);
      currentNodes[id][enclosing] = current;
    } else {
      current = it->second;
    }
    return current;
  }
}
