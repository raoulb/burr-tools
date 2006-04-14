/* Burr Solver
 * Copyright (C) 2003-2006  Andreas R�ver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "disassembly.h"

#include <xmlwrapp/attributes.h>

#include <string.h>

/* template function to get space separated integer values
 * from a string and enter these into an iterator
 * the count of numbers need to exactly fill the range
 * defined by the 2 iterators
 */
template<typename iter>
void getNumbers(const char * c, iter start, iter end, bool neg_allowed) {

  int val = 0;
  bool gotNum = false;
  bool negative = false;

  while (*c) {

    if (*c == '-' && neg_allowed) {

      // only one - and not number must have been there
      if (negative || gotNum)
        throw load_error("too many '-' signs in disassembly number");

      negative = true;

    } else if ((*c >= '0') && (*c <= '9')) {

      val = val * 10 + *c - '0';
      gotNum = true;

    } else if (*c == ' ') {

      if (gotNum) {

        if (start == end)
          throw load_error("too many numbers in disassembly");

        if (negative) val = -val;
        *start = val;
        start++;
        val = 0;
        gotNum = negative = false;
      }

      if (negative)
        throw load_error("only '-' encountered in disassembly");

    } else {

      throw load_error("not alowed character in disassembly");

    }
    c++;
  }

  /* if we have got a last number, enter it
   * this happens, when there is no space at the end of
   * the list of numbers
   */
  if (gotNum) {

    if (start == end)
      throw load_error("too many numbers in disassembly");

    if (negative) val = -val;
    *start = val;
    start++;
  }

  // check, if we filled the range
  if (start != end)
    throw load_error("too few number in disassembly");
}

/************************************************************************
 * State
 ************************************************************************/

xml::node state_c::save(unsigned int piecenumber) const {

  xml::node nd("state");
  std::string w;
  xml::node::iterator it;
  char tmp[50];

  it = nd.insert(xml::node("dx"));

  for (unsigned int ii = 0; ii < piecenumber; ii++) {
    if (w.length()) w += ' ';
    snprintf(tmp, 49, "%i", dx[ii]);
    w += tmp;
  }
  it->set_content(w.c_str());

  w = "";
  it = nd.insert(xml::node("dy"));

  for (unsigned int ii = 0; ii < piecenumber; ii++) {
    if (w.length()) w += ' ';
    snprintf(tmp, 49, "%i", dy[ii]);
    w += tmp;
  }
  it->set_content(w.c_str());

  w = "";
  it = nd.insert(xml::node("dz"));

  for (unsigned int ii = 0; ii < piecenumber; ii++) {
    if (w.length()) w += ' ';
    snprintf(tmp, 49, "%i", dz[ii]);
    w += tmp;
  }
  it->set_content(w.c_str());

  return nd;
}

state_c::state_c(const xml::node & node, unsigned int pn) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "state") != 0))
    throw load_error("not the right node for disassembly state", node);

  if ((node.find("dx") == node.end()) ||
      (node.find("dy") == node.end()) ||
      (node.find("dz") == node.end()))
    throw load_error("disassembly state needs dx, dy and dz subnode", node);

  dx = new int[pn];
  dy = new int[pn];
  dz = new int[pn];

  if (!dx || !dy || !dz)
    throw load_error("could not allocate memory for disassembly state", node);

#ifndef NDEBUG
  piecenumber = pn;
#endif

  try {
    getNumbers(node.find("dx")->get_content(), dx, dx+pn, true);
    getNumbers(node.find("dy")->get_content(), dy, dy+pn, true);
    getNumbers(node.find("dz")->get_content(), dz, dz+pn, true);
  }
  catch (load_error e) {
    throw load_error(e.getText(), node);
  }
}

state_c::state_c(unsigned int pn)
#ifndef NDEBUG
: piecenumber(pn)
#endif
{
  dx = new int[pn];
  dy = new int[pn];
  dz = new int[pn];
  bt_assert(dx && dy && dz);
}

state_c::~state_c() {
  delete [] dx;
  delete [] dy;
  delete [] dz;
}

void state_c::set(unsigned int piece, int x, int y, int z) {
  bt_assert(piece < piecenumber);
  dx[piece] = x;
  dy[piece] = y;
  dz[piece] = z;
}

bool state_c::pieceRemoved(unsigned int i) const {
  bt_assert(i < piecenumber);
  return (abs(dx[i]) > 10000) || (abs(dy[i]) > 10000) || (abs(dz[i]) > 10000);
}

/************************************************************************
 * Separation
 ************************************************************************/

xml::node separation_c::save(void) const {

  xml::node nd("separation");

  xml::node::iterator it;
  char tmp[50];

  // first save the pieces array
  it = nd.insert(xml::node("pieces"));

  snprintf(tmp, 50, "%i", piecenumber);
  it->get_attributes().insert("count", tmp);

  std::string cont;

  for (unsigned int ii = 0; ii < piecenumber; ii++) {
    snprintf(tmp, 50, "%i", pieces[ii]);
    if (cont.length()) cont += ' ';
    cont += tmp;
  }
  it->set_content(cont.c_str());

  // now add all the states
  for (unsigned int jj = 0; jj < states.size(); jj++)
    nd.insert(states[jj]->save(piecenumber));

  // finally save the removed and left over part
  // we ann an attribute to the node of the subseparations to later distinguish
  // between the removed and the left over separation
  if (removed)
    nd.insert(removed->save())->get_attributes().insert("type", "removed");

  if (left)
    nd.insert(left->save())->get_attributes().insert("type", "left");

  return nd;
}

separation_c::separation_c(const xml::node & node, unsigned int pieceCnt) {

  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "separation") != 0))
    throw load_error("wrong node for separation", node);

  if (node.find("pieces") == node.end())
    throw load_error("separation needs subnode 'pieces'", node);

  if (node.find("state") == node.end())
    throw load_error("separation needs subnode 'state'", node);

  xml::node::const_iterator it;

  // load the pieces array
  it = node.find("pieces");

  if (it->get_attributes().find("count") == it->get_attributes().end())
    throw load_error("pieces node needs a 'count' attribute", *it);

  piecenumber = atoi(it->get_attributes().find("count")->get_value());

  if (piecenumber != pieceCnt)
    throw load_error("the number of pieces in the count array is not as expected", *it);

  pieces = new voxel_type[piecenumber];

  if (pieces == 0)
    throw load_error("could not allocate the required memors", *it);

  try {
    getNumbers(it->get_content(), pieces, pieces+piecenumber, false);
  }
  catch (load_error e) {
    throw load_error(e.getText(), node);
  }

  // get the states
  for (it = node.begin(); it != node.end(); it++) {
    if ((it->get_type() == xml::node::type_element) &&
        (strcmp(it->get_name(), "state") == 0))
      states.push_back(new state_c(*it, piecenumber));
  }

  unsigned int removedPc = 0, leftPc = 0;

  for (unsigned int i = 0; i < pieceCnt; i++)
    if (states[states.size()-1]->pieceRemoved(i))
      removedPc++;
    else
      leftPc++;

  if ((removedPc == 0) || (leftPc == 0))
    throw load_error("there need to be pieces in both parts of the tree", node);

  // get the left and removed subseparation
  removed = left = 0;

  for (it = node.begin(); it != node.end(); it++) {
    if ((it->get_type() == xml::node::type_element) &&
        (strcmp(it->get_name(), "separation") == 0)) {

      bt_assert(it->get_attributes().find("type") != it->get_attributes().end());
      if (!strcmp(it->get_attributes().find("type")->get_value(), "left")) {
        if (left)
          throw load_error("more than one left branche in disassembly", node);
        left = new separation_c(*it, leftPc);
      } else if (!strcmp(it->get_attributes().find("type")->get_value(), "removed")) {
        if (removed)
          throw load_error("more than one removed branche in disassembly", node);
        removed = new separation_c(*it, removedPc);
      } else
        throw load_error("subnodes most be either left or removed", node);
    }
  }
}

separation_c::separation_c(separation_c * r, separation_c * l, unsigned int pn, voxel_type * pcs) : piecenumber(pn), removed(r), left(l) {
  pieces = new voxel_type[pn];
  memcpy(pieces, pcs, pn*sizeof(voxel_type));
}

separation_c::~separation_c() {
  delete removed;
  delete left;
  for (unsigned int i = 0; i < states.size(); i++)
    delete states[i];
  delete [] pieces;
}

unsigned int separation_c::sumMoves(void) const {
  bt_assert(states.size());
  unsigned int erg = states.size() - 1;
  if (removed)
    erg += removed->sumMoves();
  if (left)
    erg += left->sumMoves();

  return erg;
}

void separation_c::addstate(state_c *st) {
  bt_assert(st->piecenumber == piecenumber);
  states.push_front(st);
}

bool separation_c::containsMultiMoves(void) {
  return (states.size() > 2) ||
    (left && left->containsMultiMoves()) ||
    (removed && removed->containsMultiMoves());
}

int separation_c::movesText(char * txt, int len) {
  int len2 = snprintf(txt, len, "%i", states.size()-1);

  if (len2+5 > len)
    return len2;

  if (left && left->containsMultiMoves()) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += left->movesText(txt+len2, len-len2);
  }

  if (len2+5 > len)
    return len2;

  if (removed && removed->containsMultiMoves()) {
    snprintf(txt+len2, len-len2, ".");
    len2++;
    len2 += removed->movesText(txt+len2, len-len2);
  }

  return len2;
}

int separation_c::compare(const separation_c * s2) const {

  if (!s2) return 1;

  if (states.size() > s2->states.size())
    return 1;
  else if (states.size() < s2->states.size())
    return -1;
  else {
    int a;
    if (left)
      a = left->compare(s2->left);
    else if (s2->left)
      a = -1;
    else
      a = 0;

    if (a != 0) return a;

    if (removed)
      return removed->compare(s2->removed);
    else if (s2->removed)
      return -1;
    else
      return 0;
  }
}


void separation_c::shiftPiece(unsigned int pc, int dx, int dy, int dz) {
  for (unsigned int p = 0; p < piecenumber; p++)
    if (pieces[p] == pc)
      for (unsigned int s = 0; s < states.size(); s++)
  	states[s]->set(p, states[s]->getX(p)+dx, states[s]->getY(p)+dy, states[s]->getZ(p)+dz);

  if (removed)
    removed->shiftPiece(pc, dx, dy, dz);

  if (left)
    left->shiftPiece(pc, dx, dy, dz);
}

void separation_c::exchangeShape(unsigned int s1, unsigned int s2) {

  for (unsigned int i = 0; i < piecenumber; i++)
    if (pieces[i] == s1) pieces[i] = s2;
    else if (pieces[i] == s2) pieces[i] = s1;

  if (removed)
    removed->exchangeShape(s1, s2);

  if (left)
    left->exchangeShape(s1, s2);
}
