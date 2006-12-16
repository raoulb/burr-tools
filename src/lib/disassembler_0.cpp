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
#include "disassembler_0.h"

#include "disassembly.h"
#include "bt_assert.h"
#include "movementcache.h"
#include "puzzle.h"
#include "grouping.h"
#include "assembly.h"

#include <queue>

/* node is used to build the search tree for the breadth first search of the
 * state tree each node contains the position of all the pieces relative
 * to their position in the assembled puzzle
 */
class node0_c {

private:

  /* the nodes are used to save the shortest way from the start to each
   * node. So each node saves where the way to the start is
   */
  node0_c * comefrom;

  /* number of pieces this node is handling */
  int piecenumber;

  /* the position of the piece inside the cube
   * and the transformation of that piece
   */
  int *dx, *dy, *dz;
  unsigned int *trans;

  /* contains the number of pointers that point to this node
   * most of there pointers are comefrom pointers from other nodes
   * but one increment results from the pointer inside the node lists
   */
  unsigned int refcount;

public:

  node0_c(int pn, node0_c * comf) : comefrom(comf), piecenumber(pn), refcount(1) {
    dx = new int[piecenumber];
    dy = new int[piecenumber];
    dz = new int[piecenumber];
    trans = new unsigned int[piecenumber];

    if (comefrom)
      comefrom->refcount++;
  }

  ~node0_c() {
    delete [] dx;
    delete [] dy;
    delete [] dz;
    delete [] trans;

    if (comefrom) {
      comefrom->refcount--;
      if (comefrom->refcount == 0)
        delete comefrom;
    }
  }

  /* this function is used by outsiders to free
   * their own pointers to this node, if the function
   * returs true, delete the node
   */
  bool decRefCount(void) {
    bt_assert(refcount > 0);
    refcount--;
    return refcount == 0;
  }

  unsigned int hash(void) const {
    unsigned int h = 0x17fe3b3c;

    for (int i = 1; i < piecenumber; i++) {
      h += (dx[i]-dx[0]);
      h *= 1343;
      h += (dy[i]-dy[0]);
      h *= 923;
      h += (dz[i]-dz[0]);
      h *= 113;
    }

    return h;
  }

  /* the comparison operations use "normalized" positions,
   * meaning all the pieces are shifted so, that the position
   * of piece 0 is (0; 0; 0). This prevents us from shifting
   * the whole set around without movement of the pieces
   * relative to each other because all nodes with all pieces
   * shifted by the same mount do are equal
   */
  bool operator == (const node0_c &b) const {

    for (int i = 1; i < piecenumber; i++) {
      if (dx[i] - dx[0] != b.dx[i] - b.dx[0]) return false;
      if (dy[i] - dy[0] != b.dy[i] - b.dy[0]) return false;
      if (dz[i] - dz[0] != b.dz[i] - b.dz[0]) return false;
      // FIXME: transformation is missing
    }

    return true;
  }

  int getX(int i) const {
    bt_assert(i < piecenumber);
    return dx[i];
  }
  int getY(int i) const {
    bt_assert(i < piecenumber);
    return dy[i];
  }
  int getZ(int i) const {
    bt_assert(i < piecenumber);
    return dz[i];
  }
  unsigned int getTrans(int i) const {
    bt_assert(i < piecenumber);
    return trans[i];
  }
  void set(int i, int x, int y, int z, unsigned int tr) {
    bt_assert(i < piecenumber);
    dx[i] = x;
    dy[i] = y;
    dz[i] = z;
    trans[i] = tr;
  }

  /* check if the given piece is at a position outside
   * of the rest of the puzzle
   */
  bool is_piece_removed(int nr) const {
    bt_assert(nr < piecenumber);
    return ((abs(dx[nr]) > 10000) || (abs(dy[nr]) > 10000) || (abs(dz[nr]) > 10000));
  }

  /* check if this node is for a state that separates
   * the puzzle into 2 pieces. This is the case if there
   * is one piece that is removed
   */
  bool is_separation() const {
    for (int i = 0; i < piecenumber; i++)
      if (is_piece_removed(i))
        return true;

    return false;
  }

  const node0_c * getComefrom(void) const {
    return comefrom;
  }

  /* for the links in the hashtable we use this
   * pointer
   */
  node0_c * next;
};

/* this is a hashtable that stores nodes */
class nodeHash {

  private:

    unsigned long tab_size;
    unsigned long tab_entries;

    node0_c ** tab;

  public:

    nodeHash(void) {

      tab_size = 11;
      tab_entries = 0;

      tab = new node0_c* [tab_size];

      memset(tab, 0, tab_size*sizeof(node0_c*));
    }

    ~nodeHash(void) {
      clear(false);

      delete [] tab;
    }

    /* delete all nodes and empty table for new usage */
    void clear(bool reset = true) {
      for (unsigned int i = 0; i < tab_size; i++) {
        while (tab[i]) {
          node0_c * n = tab[i];
          tab[i] = n->next;

          if (n->decRefCount())
            delete n;
        }
      }

      if (reset) {
        memset(tab, 0, tab_size*sizeof(node0_c*));
        tab_entries = 0;
      }
    }

    /* add a new node  returns true, if the given node has already been
     * in the table, false if the node is inserted
     */
    bool insert(node0_c * n) {

      unsigned long h = n->hash() % tab_size;

      node0_c * hn = tab[h];

      while (hn) {
        if (*hn == *n)
          return true;

        hn = hn->next;
      }

      /* node not in table, insert */

      n->next = tab[h];
      tab[h] = n;

      tab_entries++;
      if (tab_entries > tab_size) {
        // rehash

        unsigned long new_size = tab_size * 4 + 1;

        node0_c ** new_tab = new node0_c* [new_size];
        memset(new_tab, 0, new_size*sizeof(node0_c*));

        for (unsigned int i = 0; i < tab_size; i++) {
          while (tab[i]) {
            node0_c * n = tab[i];
            tab[i] = n->next;
            unsigned long h = n->hash() % new_size;
            n->next = new_tab[h];
            new_tab[h] = n;
          }
        }

        delete[] tab;
        tab = new_tab;
        tab_size = new_size;
      }

      return false;
    }

    /* check, if a node is in the map */
    bool contains(node0_c * n) {
      unsigned long h = n->hash() % tab_size;

      node0_c * hn = tab[h];

      while (hn) {
        if (*hn == *n)
          return true;

        hn = hn->next;
      }

      return false;
    }

};

/* so, this isn't the function as described by Bill but rather a
 * bit optimized. For each pair of 2 different pieces and for
 * each of the three dimensions I do the following:
 *  - check the intersection area area in this direction
 *  - if it's empty the pieces do not interlock and the matrix
 *    is initialized to infinity (32000)
 *  - if we have an intersection we check each column inside this area
 *    and find the shortest distance the first piece follows
 *    the second and the second piece follows the first
 */
void disassembler_0_c::prepare(int pn, voxel_type * pieces, node0_c * searchnode) {

  unsigned int idx = 0;
  for (int j = 0; j < pn; j++) {
    for (int i = 0; i < pn; i++) {
      if (i != j)
        cache->getValue(searchnode->getX(j) - searchnode->getX(i),
                        searchnode->getY(j) - searchnode->getY(i),
                        searchnode->getZ(j) - searchnode->getZ(i),
                        searchnode->getTrans(i), searchnode->getTrans(j),
                        pieces[i], pieces[j],
                        &matrix[0][idx], &matrix[1][idx], &matrix[2][idx]);

      // the diagonals are always zero and will stay that for ever they are initialized
      // to that value in the init function so only the other values need
      idx++;
    }
    idx = idx - pn + piecenumber;
  }
}

void disassembler_0_c::prepare2(int pn) {

  /* having a look at this algorithm in more detail
   * it comes out that the first pass has lots to do, the 2nd pass
   * is much cheaper (usually a few more corrections sometimes
   * event zero) and then it finished
   *
   * so I change this: alsways look, if a change done leads to other necessary
   * changes, and only if that is the case do another loop
   */

  /* second part of Bills algorithm. */

  unsigned int again = 0;

  for (int d = 0; d < 3; d++) {
    do {
      again = 0;

/*
      // this is just for commentaty reasons it show the same algorithmus as below
      // just a bit more understandable

      for (int y = 0; y < pn; y++)
        for (int x = 0; x < pn; x++) {
          int min = matrix[d][x] + matrix[d][y*piecenumber];

          for (int i = 1; i < pn; i++) {
            int l = matrix[d][x + i*piecenumber] + matrix[d][i + y*piecenumber];
            if (l < min) min = l;
          }

          if (min < matrix[d][x + y*piecenumber]) {
            matrix[d][x + y*piecenumber] = min;
            if (!again) {
              for (int i = 0; i < y; i++)
                if (min + matrix[d][y + i*piecenumber] < matrix[d][x + i*piecenumber]) {
                  again = true;
                  break;
                }

              if (!again)
                for (int i = 0; i < x; i++)
                  if (matrix[d][i + x*piecenumber] + min < matrix[d][i + y*piecenumber]) {
                    again = true;
                    break;
                  }
          }
        }
*/

#if 1
      int * pos1 = matrix[d];           // y * piecenumber;
      int idx, i;

      for (int y = 0; y < pn; y++) {
        int * pos2 = matrix[d];           // x

        for (int x = 0; x < pn; x++) {
          int min = *pos2 + *pos1;

          for (i = 1, idx = piecenumber; i < pn; i++, idx += piecenumber) {
            int l = pos2[idx] + pos1[i];
            if (l < min) min = l;
          }

          if (min < pos1[x]) {
            pos1[x] = min;

            if (!again) {

              int * pos3 = matrix[d];

              for (int i = 0; i < y; i++) {
                if (min + pos3[y] < pos3[x]) {
                  again = true;
                  break;
                }
                pos3 += piecenumber;
              }

              if (!again) {

                pos3 = matrix[d] + x*piecenumber;

                for (int i = 0; i < x; i++)
                  if (pos3[i] + min < pos1[i]) {
                    again = true;
                    break;
                  }
              }
            }
          }
          pos2++;
        }

        pos1 += piecenumber;
      }
#endif

    } while (again > 0);
  }
}

/*
 * suppose you want to move piece x y units into one direction, if you hit another piece
 * on your way and this piece can be moved then it may be nice to also move this piece
 *
 * so this function adjusts the movement of other pieces so that one piece can be moved
 * the requested number of units.
 *
 * in the worst case when no movement in the selected direction is possible all values are
 * set to the same value meaning the whole puzzle is moved
 *
 * to distinguish "good" and "bad" moves the function returns true, if less maxPieces
 * have to be moved, this value should not be larger than halve of the pieces in the puzzle
 */
bool disassembler_0_c::checkmovement(unsigned int maxPieces, int nextdir, int next_pn, int nextpiece, int nextstep) {

  /* we count the number of pieces that need to be moved, if this number
   * gets bigger than halve of the pieces of the current problem we
   * stop and return that this movement is rubbish
   */
  unsigned int moved_pieces = 1;

  /* initialize the movement matrix. We want to move 'nextpiece' 'nextstep' units
   * into the current direction, so we initialize the matrix with all
   * zero except for our piece
   */
  for (int i = 0; i < next_pn; i++) {
    movement[i] = 0;
    check[i] = false;
  }
  movement[nextpiece] = nextstep;
  check[nextpiece] = true;

  bool finished;
  int nd = nextdir >> 1;

  if (nextdir & 1) {

    do {

      finished = true;

      for (int i = 0; i < next_pn; i++)
        if (check[i]) {
          for (int j = 0; j < next_pn; j++)
            if ((i != j) && (movement[j] == 0)) {
              if ((movement[i] - matrix[nd][j + piecenumber * i]) > 0) {
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
          check[i] = false;
        }

    } while (!finished);

  } else {

    do {

      finished = true;

      for (int i = 0; i < next_pn; i++)
        if (check[i]) {
          for (int j = 0; j < next_pn; j++)
            if ((i != j) && (movement[j] == 0)) {
              if ((movement[i] - matrix[nd][i + piecenumber * j]) > 0) {
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
          check[i] = false;
        }

    } while (!finished);
  }

  return true;
}

void disassembler_0_c::init_find(node0_c * nd, int piecenumber, voxel_type * pieces) {

  /* when a new search has been started we need to first calculate
   * the movement matrices, this is a table that contains one 2 dimensional
   * matrix for each of the 6 directions where movement is possible
   *
   * the matrices contains possible movement of one piece if other pieces
   * are not moved. So a one in column 2 row 4 means that piece nr. 2 can
   * be moved one unit it we fix piece nr. 4
   *
   * the algorithm used here is describes in Bill Cuttlers booklet
   * "Computer Analysis of All 6 Piece Burrs"
   */
  prepare(piecenumber, pieces, nd);
  prepare2(piecenumber);

  /* first we try to remove the piece completely by specifying
   * a very large distance, if it is possible to move this piece that
   * far, it is probably removed
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  nextstate = 0;
  next_pn = piecenumber;
}

static int max(int a, int b) {
  if (a > b)
    return a;
  else
    return b;
}

static node0_c * newNode(int next_pn, int nextdir, node0_c * searchnode, int * movement, const int * weights) {

  // we only take this new node, when all pieces are either not moved at all
  // or moved by the same amount

  int amount = 0;
  int moveWeight = 0;
  int stilWeight = 0;

  for (int i = 0; i < next_pn; i++) {
    if (movement[i]) {
      if (amount == 0)
        amount = movement[i];

      bt_assert(amount == movement[i]);

      moveWeight = max(moveWeight, weights[i]);

    } else {
      stilWeight = max(stilWeight, weights[i]);
    }
  }

  node0_c * n = new node0_c(next_pn, searchnode);

  /* we move the group of pieces with the smaller weight or, if both have the same
   * weight the group with the fewer pieces.
   */
  if (stilWeight >= moveWeight) {

    /* create a new state with the pieces moved */
    for (int i = 0; i < next_pn; i++)
      switch(nextdir) {
        case 0: n->set(i, searchnode->getX(i) + movement[i], searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 1: n->set(i, searchnode->getX(i) - movement[i], searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 2: n->set(i, searchnode->getX(i), searchnode->getY(i) + movement[i], searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 3: n->set(i, searchnode->getX(i), searchnode->getY(i) - movement[i], searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 4: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) + movement[i], searchnode->getTrans(i)); break;
        case 5: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) - movement[i], searchnode->getTrans(i)); break;
      }

  } else {

    /* here we do the opposite of above, all pieces that are steady in the movement field are moved
     * by the negative amount and the moving pieces are left untouched
     */
    for (int i = 0; i < next_pn; i++)
      switch(nextdir) {
        case 0: n->set(i, searchnode->getX(i) + (movement[i]?0:(-amount)), searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 1: n->set(i, searchnode->getX(i) - (movement[i]?0:(-amount)), searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 2: n->set(i, searchnode->getX(i), searchnode->getY(i) + (movement[i]?0:(-amount)), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 3: n->set(i, searchnode->getX(i), searchnode->getY(i) - (movement[i]?0:(-amount)), searchnode->getZ(i), searchnode->getTrans(i)); break;
        case 4: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) + (movement[i]?0:(-amount)), searchnode->getTrans(i)); break;
        case 5: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) - (movement[i]?0:(-amount)), searchnode->getTrans(i)); break;
      }
  }

  return n;
}


/* creates a new node that contains the merged movements of the given 2 nodes */
static node0_c * newNodeMerge(const node0_c *n0, const node0_c *n1, node0_c * searchnode, int next_pn, int nextdir, int * movement, const int * weights) {

  /* we need to make sure the new node is different from n0 and n1
   */
  bool different0 = false;
  bool different1 = false;
  int moved = 0;

  int adder0, adder1;
  adder0 = adder1 = 0;

  int diff;

  // first find out, for both nodes, if they are positive or negative movement nodes
  // this can happen because of the weights that we have negative movement even though
  // we are right now positive axis and also the other way around
  for (int i = 0; i < next_pn; i++) {
    switch(nextdir) {
      case 0: diff = searchnode->getX(i) - n0->getX(i); break;
      case 1: diff = n0->getX(i) - searchnode->getX(i); break;
      case 2: diff = searchnode->getY(i) - n0->getY(i); break;
      case 3: diff = n0->getY(i) - searchnode->getY(i); break;
      case 4: diff = searchnode->getZ(i) - n0->getZ(i); break;
      case 5: diff = n0->getZ(i) - searchnode->getZ(i); break;
      default:
        bt_assert(0);
        break;
    }

    if (diff > 0) {
      adder0 = diff;
      break;
    }
  }

  for (int i = 0; i < next_pn; i++) {
    switch(nextdir) {
      case 0: diff = searchnode->getX(i) - n1->getX(i); break;
      case 1: diff = n1->getX(i) - searchnode->getX(i); break;
      case 2: diff = searchnode->getY(i) - n1->getY(i); break;
      case 3: diff = n1->getY(i) - searchnode->getY(i); break;
      case 4: diff = searchnode->getZ(i) - n1->getZ(i); break;
      case 5: diff = n1->getZ(i) - searchnode->getZ(i); break;
      default:
        bt_assert(0);
        break;
    }

    if (diff > 0) {
      adder1 = diff;
      break;
    }
  }

  int amount = 0;
  int d0, d1;

  for (int i = 0; i < next_pn; i++) {

    // calculate the movement of the merged node
    switch(nextdir) {
      case 0:
      case 1:
        d0 = abs(n0->getX(i) - searchnode->getX(i)) + adder0;
        d1 = abs(n1->getX(i) - searchnode->getX(i)) + adder1;
        break;
      case 2:
      case 3:
        d0 = abs(n0->getY(i) - searchnode->getY(i)) + adder0;
        d1 = abs(n1->getY(i) - searchnode->getY(i)) + adder1;
        break;
      case 4:
      case 5:
        d0 = abs(n0->getZ(i) - searchnode->getZ(i)) + adder0;
        d1 = abs(n1->getZ(i) - searchnode->getZ(i)) + adder1;
        break;
      default:
        bt_assert(0);
        break;
    }
    movement[i] = (d0 > d1) ? d0 : d1;
    different0 |= (movement[i] != d0);
    different1 |= (movement[i] != d1);
    if (movement[i]) {
      moved++;
      if (amount == 0)
        amount = movement[i];
      else if (movement[i] != amount)
        return 0;
    }
  }

  // if the new node is equal to n0 or n1, exit
  if (!different0 || !different1) return 0;

  return newNode(next_pn, nextdir, searchnode, movement, weights);
}


/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 */
node0_c * disassembler_0_c::find(node0_c * searchnode, const int * weights) {

  node0_c * n = 0;

  static std::vector<node0_c *> nodes;

  // repeat until we either find a movement or have checked everything
  while (!n && ((nextstate < 3) || (nextstate == 99))) {

    switch (nextstate) {
      case 0:
        // check, if a single piece can be removed
        if (checkmovement(1, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, movement, weights);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 6) {
            nextstate++;
            nextdir = 0;
          }
        }
        break;
      case 1:
        // check, if a group of pieces can be removed
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, movement, weights);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 6) {
            nextstate++;
            nextdir = 0;
            nodes.clear();
          }
        }
        break;
      case 2:
        // check, if a single piece can be moved
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, nextstep)) {
          n = newNode(next_pn, nextdir, searchnode, movement, weights);

          // we need to merge the gained node with all already found
          // nodes with the same step and if that leads to valid new nodes
          // we also need to return those

          // but first we check, if we have this node already found (maybe via a merger)
          // and if so we delete it
          for (unsigned int i = 0; i < nodes.size(); i++)
            if (*n == *(nodes[i])) {
              delete n;
              n = 0;
              break;
            }

          // if the node is still valid save it and do a merger with all known nodes
          if (n) {

            nodes.push_back(n);

            nextstate = 99;
            state99piece = 0;
            state99piece2 = nodes.size()-1;
            state99nextState = 2;
          }

          // if we can move something, we try larger steps
          nextstep++;

        } else {

          // if not, lets try the next piece
          nextstep = 1;
          nextpiece++;
          if (nextpiece >= next_pn) {
            nextpiece = 0;
            nextdir++;
            nodes.clear();
            if (nextdir >= 6) {
              nextstate ++;
            }
          }
        }
        break;

      case 99:

        // this is a special state that takes the last found node and creates mergers with all
        // the already found nodes.
        // a merger is a new node that contains the movement of one node AND the movement of
        // the 2nd node at the same time. Of course both nodes need to point into the same
        // direction and in both nodes the pieces need to be moved by
        // the same amount
        //
        // This is needed because when moving groups of pieces and both pieces are independent of
        // one another the code above alone wont find movements where both pieces are moved at
        // the same time but rather one after the other

        if (state99piece < state99piece2) {
          n = newNodeMerge(nodes[state99piece], nodes[state99piece2], searchnode, next_pn, nextdir, movement, weights);

          // if the node is valid check if we already know that node, if so
          // delte it
          if (n) {
            for (unsigned int i = 0; i < nodes.size(); i++)
              if (*n == *(nodes[i])) {
                delete n;
                n = 0;
                break;
              }
          }

          if (n) nodes.push_back(n);

          state99piece++;
        } else
          nextstate = state99nextState;

        break;

      default:
        // endstate, do nothing
        break;
    }
  }

  return n;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(node0_c * st, node0_c ** n, voxel_type ** pn, int ** nw, int piecenumber, voxel_type * pieces, const int * weights, int part, bool cond) {

  *n = new node0_c(part, 0);
  *pn = new voxel_type[part];
  *nw = new int[part];

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond) {
      if (num == 0) {
        /* find the direction, the first piece was moved out of the puzzle
         * and shift it back along this axis */
        if ((st->getX(i) > 10000) || (st->getX(i) < -10000)) dx = st->getX(i);
        if ((st->getY(i) > 10000) || (st->getY(i) < -10000)) dy = st->getY(i);
        if ((st->getZ(i) > 10000) || (st->getZ(i) < -10000)) dz = st->getZ(i);
      }
      (*n)->set(num,
                st->getX(i) - dx,
                st->getY(i) - dy,
                st->getZ(i) - dz,
                st->getTrans(i));
      (*pn)[num] = pieces[i];
      (*nw)[num] = weights[i];
      num++;
    }

  bt_assert(num == part);
}

unsigned short disassembler_0_c::subProbGroup(node0_c * st, voxel_type * pn, bool cond, int piecenumber) {

  unsigned short group = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond)
      if (puzzle->probGetShapeGroupNumber(problem, piece2shape[pn[i]]) != 1)
        return 0;
      else if (group == 0)
        group = puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0);
      else if (group != puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0))
        return 0;

  return group;
}

bool disassembler_0_c::subProbGrouping(voxel_type * pn, int piecenumber) {

  groups->newSet();

  for (int i = 0; i < piecenumber; i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

separation_c * disassembler_0_c::checkSubproblem(int pieceCount, voxel_type * pieces, int piecenumber, node0_c * st, bool left, bool * ok, const int * weights) {

  separation_c * res = 0;

  if (pieceCount == 1) {
    *ok = true;
  } else if (subProbGroup(st, pieces, left, piecenumber)) {
    *ok = true;
  } else {

    node0_c *n;
    voxel_type * pn;
    int * nw;
    create_new_params(st, &n, &pn, &nw, piecenumber, pieces, weights, pieceCount, left);
    res = disassemble_rec(pieceCount, pn, n, nw);

    *ok = res || subProbGrouping(pn, pieceCount);

    delete [] pn;
    delete [] nw;
  }

  return res;
}

/* this is a breadth first search function that analyses the movement of
 * an assembled problem. When the problem falls apart into 2 pieces the function
 * calls itself recursively. It returns null if the problem can not be taken apart
 * completely and otherwise the disassembly tree
 *
 * the parameters are:
 *  - piecenumber: the number of pieces of the current problem. Because we can have
 *                 subproblems this number is not identical to the number of pieces
 *                 in the assembly voxel space
 *  - pieces: defines which pieces of the assembly voxel space are actually really present
 *            in the current subproblem
 *  - start: the start position of each piece
 *
 * the function takes over the ownership of the node and pieces. They are deleted at the end
 * of the function, so you must allocate them with new
 */
separation_c * disassembler_0_c::disassemble_rec(int piecenumber, voxel_type * pieces, node0_c * start, const int * weights) {

  std::queue<node0_c *> openlist[2];
  nodeHash closed[3];
  std::vector<node0_c *> deletelist;

  int curListFront = 0;
  int newListFront = 1;
  int oldFront = 0;
  int curFront = 1;
  int newFront = 2;

  closed[curFront].insert(start);
  openlist[curListFront].push(start);

  separation_c * erg = 0;

  /* the algorithm works with 3 sets of nodes. All nodes in one set do have the same distance
   * from the start node. The 3 sets are the current frontier (cf), the new frontier (nf) and the line
   * behind the current frontier (of).
   * The nodes in the cf list are taken one by one and examined and all neighbour nodes are generated. Now
   * there are 3 possibilities: 1) the node is one way further from the start, then the new node belongs to
   * nf, if the node has the same distance from the start as the currently examined node, if belongs into the
   * cf list and if it is nearer to the start is must be in of.
   * How do we find out where it belongs? By checking, if that node is already there. We check if the node is
   * in of or in cf or in nf, only if it is nowhere to be found, we add it to nf.
   * After all nodes in cf are checked, all nodes in of are dropped, cf becomes of, and nf becomes cf. nf is empty.
   * And we restart.
   * "Dropping" of the old frontier doesn't mean the nodes are deleted. They can only be deleted, when they are not
   * part of the used shortest way to one of the nodes in cf. This is done by reference counting. So dropping
   * means, check if there is an other node using a node, if not we can delete the node. This deletion may even
   * result in more deletion as this might drop the reference counter of another node to 0
   * This reference counter is increased for each node whose comefrom pointer points to that node. The counter
   * is also increased by one as long as it is inside one of the frontier lists.
   */

  /* while there are nodes left we should look at and we have not found a solution */
  while (!openlist[curListFront].empty()) {

    /* remove the node from the open list and start examining */
    node0_c * node = openlist[curListFront].front();
    openlist[curListFront].pop();

    /* if the current list is now empty, we need to toggle everything after we have finished
     * searching this node
     */

    init_find(node, piecenumber, pieces);

    node0_c * st;

    while ((st = find(node, weights))) {

      /* check all closed nodelists and also insert the node into the newFront list
       * insert has the same return value as contains, but also inserts the node
       * when it is not yet inside the hashtable
       */
      if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {

        /* the new node is already here. We have found a new longer way to that
         * node, so we can safely delete the new node and continue to the next
         *
         * don't delete it right here, but only after the search was finished because
         * the find function still requires the node information
         */
        deletelist.push_back(st);
        continue;
      }

      if (!st->is_separation()) {

        /* the new node is no solution so insert the
         * new state into the known state table
         * and the open list for later examination and go on to the next node
         */
        openlist[newListFront].push(st);
        continue;
      }

      /* if we get here we have found a node that separated the puzzle into
       * 2 pieces. So we recursively solve the subpuzzles and create a tree
       * with them that needs to be returned
       */

      /* count the pieces in both parts */
      int part1 = 0, part2 = 0;

      for (int i = 0; i < piecenumber; i++)
        if (st->is_piece_removed(i))
          part2++;
        else
          part1++;

      /* each subpart must contain at least 1 piece,
       * otherwise there is something wrong
       */
      bt_assert((part1 > 0) && (part2 > 0));

      separation_c * left, *remove;
      bool left_ok = false;
      bool remove_ok = false;
      left = remove = 0;

      /* all right, the following thing come twice, maybe I should
       * put it into a function, anyway:
       * if the subproblem to check has only one piece, it's solved
       * if all the pieces belong to the same group, we can stop
       * else try to disassemble, if that fails, try to
       * group the involved pieces into an identical group
       */
      remove = checkSubproblem(part1, pieces, piecenumber, st, false, &remove_ok, weights);

      /* only check the left over part, when the removed part is OK */
      if (remove_ok)
        left = checkSubproblem(part2, pieces, piecenumber, st, true, &left_ok, weights);

      /* if both subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if (remove_ok && left_ok) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, piecenumber, pieces);

        do {
          state_c *s = new state_c(piecenumber);

          for (int i = 0; i < piecenumber; i++)
            s->set(i, st->getX(i), st->getY(i), st->getZ(i));

          erg->addstate(s);

          st = (node0_c*)st->getComefrom();
        } while (st);

      } else {

        /* one of the subproblems was unsolvable in this case the whole
         * puzzle is unsolvable, so we can as well stop here
         */
        if (left) delete left;
        if (remove) delete remove;
      }

      /* if we get here we can stop, even if we didn't find a solution
       * so we empty the openlist and se stop the currently running
       * search process
       */

      /* nodes inside the closed hashtables are freed automagically */
      return erg;
    }

    /* now we can delete the entries inside the deletelist */
    for (unsigned int i = 0; i < deletelist.size(); i++)
      delete deletelist[i];
    deletelist.clear();

    // if the current front is completely checked, open up the new front
    if (openlist[curListFront].empty()) {

      // toggle the 2 lists of the fronts
      curListFront = 1 - curListFront;
      newListFront = 1 - newListFront;

      // free the oldFront nodes
      closed[oldFront].clear();

      oldFront = curFront;
      curFront = newFront;
      newFront = (newFront + 1) % 3;
    }
  }

  // the nodes inside the hashtables are freed automatically
  return 0;
}

disassembler_0_c::disassembler_0_c(const puzzle_c * puz, unsigned int prob) :
  piecenumber(puz->probPieceNumber(prob)), puzzle(puz), problem(prob) {

  /* allocate the necessary arrays */
  movement = new int[piecenumber];
  check = new bool[piecenumber];

  for (int j = 0; j < 3; j++)
    matrix[j] = new int[piecenumber * piecenumber];

  for (unsigned int i = 0; i < piecenumber; i++)
    matrix[0][i+i*piecenumber] = matrix[1][i+i*piecenumber] = matrix[2][i+i*piecenumber] = 0;

  cache = new movementCache_c(puzzle, problem);

  /* initialize the grouping class */
  groups = new grouping_c();
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeGroupNumber(prob, i); j++)
      groups->addPieces(puz->probGetShape(prob, i),
                        puz->probGetShapeGroup(prob, i, j),
                        puz->probGetShapeGroupCount(prob, i, j));

  /* initialize piece 2 shape transformation */
  piece2shape = new unsigned short[piecenumber];
  int p = 0;
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeCount(prob, i); j++)
      piece2shape[p++] = i;

  /* create the weights array */
  weights = new int[piecenumber];
  for (unsigned int i = 0; i < piecenumber; i++)
    weights[i] = puzzle->probGetShapeShape(prob, piece2shape[i])->getWeight();
}

disassembler_0_c::~disassembler_0_c() {
  delete [] movement;
  delete [] check;
  for (unsigned int k = 0; k < 3; k++)
    delete [] matrix[k];

  delete cache;
  delete groups;
  delete [] piece2shape;
  delete [] weights;
}

separation_c * disassembler_0_c::disassemble(const assembly_c * assembly) {

  bt_assert(piecenumber == assembly->placementCount());

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  node0_c * start = new node0_c(piecenumber, 0);

  for (unsigned int i = 0; i < piecenumber; i++)
    start->set(i, assembly->getX(i), assembly->getY(i), assembly->getZ(i), assembly->getTransformation(i));

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  voxel_type * pieces = new voxel_type[piecenumber];
  for (unsigned int j = 0; j < piecenumber; j++)
    pieces[j] = j;

  /* reset the grouping class */
  groups->reSet();

  separation_c * s = disassemble_rec(piecenumber, pieces, start, weights);

  delete [] pieces;

  return s;
}
