#include "config.h"

#include "context.hpp"
#include <cassert>

#include <cstring>
#include <cstdio>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

namespace trace {
  typedef std::chrono::nanoseconds ns;

  /* \brief pointer to a function return the worker Id (between 0 and nbWorkers-1) or -1 in a sequential section */
  int (*nodeIndexFunction)() = NULL;

  /** \brief Set the function used to get the root index.

      \return a number between 0 (not in a parallel region) and the number of
      workers (included).
   */
  static int currentNodeIndex() {
    int res = (nodeIndexFunction ? nodeIndexFunction() : -1) + 1;
    assert(res>=0 && res<MAX_ROOTS);
    return res;
  }

  void setNodeIndexFunction(int (*nodeIndexFunc)()) {
    nodeIndexFunction = nodeIndexFunc;
  }

  bool Node::enabled = true;
  std::unordered_map<void*, Node*> Node::currentNodes[MAX_ROOTS];
  void* Node::enclosingContext[MAX_ROOTS] = {};

  Node::Node(const char* _name, Node* _parent)
    : name_(_name), data(), parent(_parent), children() {}

  Node::~Node() {
    for (auto it = children.begin(); it != children.end(); ++it) {
      delete *it;
    }
  }

  void Node::enterContext(const char* name) {
    if (!enabled) return;
    Node* current = currentNode();
    assert(current);
    Node* child = current->findChild(name);
    int index = currentNodeIndex();
    void* enclosing = enclosingContext[index];

    if (!child) {
      child = new Node(name, current);
      current->children.push_back(child);
    }
    assert(child);
    currentNodes[index][enclosing] = child;
    current = child;
    current->data.lastEnterTime = std::chrono::high_resolution_clock::now();
    current->data.n += 1;
  }

  void Node::leaveContext() {
    if (!enabled) return;
    int index = currentNodeIndex();
    void* enclosing = enclosingContext[index];
    Node* current = currentNodes[index][enclosing];
    assert(current);

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
      if ((*it)->name_ == name) {
        return *it;
      }
    }
    return NULL;
  }

  void Node::jsonDump(std::ofstream& f) const {
    f << "{"
      << "\"name\": \"" << name_ << "\", "
      << "\"id\": \"" << this << "\", "
      << "\"n\": " << data.n << ", "
      << "\"totalTime\": " << data.totalTime / 1e9 << ", "
      << "\"totalFlops\": " << data.totalFlops << ", "
      << "\"totalBytesSent\": " << data.totalBytesSent << ", "
      << "\"totalBytesReceived\": " << data.totalBytesReceived << ", "
      << "\"totalCommTime\": " << data.totalCommTime / 1e9 << "," << std::endl;
    f << "\"children\": [";
    std::string delimiter("");
    for (auto it = children.begin(); it != children.end(); ++it) {
      f << delimiter;
      (*it)->jsonDump(f);
      delimiter = ", ";
    }
    f << "]}";
  }

  void Node::jsonDumpMain(const char* filename) {
    std::ofstream f(filename);

    f << "[";
    std::string delimiter("");
    for (int i = 0; i < MAX_ROOTS; i++) {
      if (!currentNodes[i].empty()) {
        for (auto p : currentNodes[i]) {
          Node* root = p.second;
          f << delimiter << std::endl;
          root->jsonDump(f);
          delimiter = ", ";
        }
      }
    }
    f << std::endl << "]" << std::endl;
  }

  /** Find the current node, allocating one if necessary.
   */
  Node* Node::currentNode() {
    int index = currentNodeIndex();
    void* enclosing = enclosingContext[index];
    auto it = currentNodes[index].find(enclosing);
    Node* current;
    if (it == currentNodes[index].end()) {
      char *name = const_cast<char*>("root");
      if (index != 0) {
        name = strdup("Worker #XXX - 0xXXXXXXXXXXXXXXXX"); // Worker ID - enclosing
        assert(name);
        // Avec toyrt, les threads 1 et 2 ne sont pas des workers, ce sont les threads IO & MPI
        if (index==1)
          sprintf(name, "Worker IO - %p", enclosing);
        else if (index==2)
          sprintf(name, "Worker MPI - %p", enclosing);
        else
          sprintf(name, "Worker #%03d - %p", index-2, enclosing);
      }
      current = new Node(name, NULL);
      currentNodes[index][enclosing] = current;
    } else {
      current = it->second;
    }
    return current;
  }
}
