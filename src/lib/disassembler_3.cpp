/* Burr Solver
 * Copyright (C) 2003-2005  Andreas R�ver
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

#include "disassembler_3.h"

#include "disassembly.h"
#include "voxel.h"

#include <queue>
#include <set>
#include <functional>

/* a macro to calculate the minimum and maximum
 * of two values in one step
 */
#define minmax(a, b, min, max) { \
  if (a > b) {                   \
    min = b;                     \
    max = a;                     \
  } else {                       \
    min = a;                     \
    max = b;                     \
  }                              \
}

// this macro look backward for the first occurence of the given piece
#define search_back(run, end, piece)                                                 \
  while ((run >= end) && (searchnode->getVoxel(assm, x, y, z, piece) != p##piece)) { \
    run--;                                                                           \
    value++;                                                                         \
  }

// this macro looks forward for the first occurence of the given piece
#define search_forward(run, end, piece)                                              \
  while ((run <= end) && (searchnode->getVoxel(assm, x, y, z, piece) != p##piece)) { \
    run++;                                                                           \
    value++;                                                                         \
  }

// this macro looks forward for the first occurence of the given piece
// without caring for the value variable
#define search_start(run, end, piece)                             \
  while (run <= end) {                                            \
    if (searchnode->getVoxel(assm, x, y, z, piece) == p##piece) { \
      value = 0;                                                  \
      run++;                                                      \
      break;                                                      \
    }                                                             \
    run++;                                                        \
  }

// this looks forward for the first occurence of the given piece but stopping
// as soon as value is bigger than minimum
#define search_forward_stop(run, end, piece)                      \
  while ((value < minimum) && (run <= end)) {                     \
    if (searchnode->getVoxel(assm, x, y, z, piece) == p##piece) { \
      if (value < minimum) minimum = value;                       \
      break;                                                      \
    }                                                             \
    run++;                                                        \
    value++;                                                      \
  }

// the normal search macro looking for the smallest gap between a piece 1
// voxel and one from piece 1
#define search_normal(run, end, piece1, piece2)                            \
  while (run <= end) {                                                     \
    if (searchnode->getVoxel(assm, x, y, z, piece1) == p##piece1) {        \
      value = 0;                                                           \
    } else if (searchnode->getVoxel(assm, x, y, z, piece2) == p##piece2) { \
      if (value < minimum) minimum = value;                                \
    } else {                                                               \
      value++;                                                             \
    }                                                                      \
    run++;                                                                 \
  }

// the whole search process with different, optimized search functions
// for each of the 6 cases
#define search(dir, Dir, d1, D1, d2, D2, p1, p2, bpbs, fsf1, fsf2)  \
  minimum = 32000;                                                  \
  for (int d1 = d##d1##1; d1 <= d##d1##2; d1++)                     \
    for (int d2 = d##d2##1; d2 <= d##d2##2; d2++) {                 \
                                                                    \
      int a1p1, a2p1, a1p2, a2p2;                                   \
                                                                    \
      a1p1 = b##dir##1[p##p1] + searchnode->get##Dir(p1);           \
      a2p1 = b##dir##2[p##p1] + searchnode->get##Dir(p1);           \
                                                                    \
      if (a1p1 > a2p1) continue;                                    \
                                                                    \
      a1p2 = b##dir##1[p##p2] + searchnode->get##Dir(p2);           \
      a2p2 = b##dir##2[p##p2] + searchnode->get##Dir(p2);           \
                                                                    \
      if ((a1p2 > a2p2) || (a1p1 > a2p2))        \
        continue;                                \
                                                 \
      if (a1p2 >= a1p1)                          \
        if (a1p2 <= a2p1)                        \
            arrangement = 3;                     \
          else                                   \
            arrangement = 6;                     \
        else                                     \
          arrangement = 0;                       \
                                                 \
      if (a2p2 >= a1p1) {                        \
        if (a2p2 <= a2p1)                        \
          arrangement += 1;                      \
        else                                     \
          arrangement += 2;                      \
      }                                          \
                                                 \
      int tc1, tc2, td1, td2;                    \
      minmax(a1p1, a1p2, tc1, td1);              \
      minmax(a2p1, a2p2, td2, tc2);              \
                                                 \
      int dir;                                   \
      switch(arrangement) {                      \
      case 1:                                    \
        value = 32000;                           \
        dir = td1;                               \
        search_start(dir, td2, p1);              \
        search_normal(dir, td2, p1, p2);         \
        break;                                   \
      case 2:                                    \
        value = 32000;                           \
        dir = td1;                               \
        search_start(dir, td2, p1);              \
        search_normal(dir, td2, p1, p2);         \
        search_forward_stop(dir, tc2, p2);       \
        break;                                   \
      case 4:                                    \
        dir = td1-1;                             \
        value = 0;                               \
        search_back(dir, tc1, p1);               \
        if (dir < tc1) value = 32000;            \
        dir = td1;                               \
        search_normal(dir, td2, p1, p2);         \
        break;                                   \
      case 5:                                    \
        dir = td1-1;                             \
        value = 0;                               \
        search_back(dir, tc1, p1);               \
        if (dir < tc1) value = 32000;            \
        dir = td1;                               \
        search_normal(dir, td2, p1, p2);         \
        search_forward_stop(dir, tc2, p2);       \
        break;                                   \
      case 8:                                    \
        value = 0;                               \
                                                 \
        dir = td2;                               \
        search_back(dir, tc1, p1);               \
        if (dir >= tc1) {                        \
                                                 \
          dir = td1;                             \
          search_forward(dir, tc2, p2);          \
          if (dir <= tc2) {                      \
                                                 \
            value += td1 - td2 - 1;              \
                                                 \
            if (value < minimum)                 \
              minimum = value;                   \
          }                                      \
        }                                        \
                                                 \
        break;                                   \
      }                                          \
                                                 \
      if (minimum == 0) {                        \
        d2 = d##d2##2+1;                         \
        d1 = d##d1##2+1;                         \
      }                                          \
    }


/* this is a special node class that can save the
 * Cuttler matrix and free it after using it
 */
class nodeWithMatrix_c : public node_c {

private:

  int nodePiecenumber;
  int * nodeMatrix[3];

public:

  nodeWithMatrix_c(int pn) : node_c(pn) { nodeMatrix[0] = 0; }

  void setMatrix(int * m[3], int piecenumber) {

    int pnsq = piecenumber * piecenumber;

    for (int i = 0; i < 3; i++) {
      nodeMatrix[i] = new int[pnsq];
      memcpy(nodeMatrix[i], m[i], sizeof(int) * pnsq);
    }

    nodePiecenumber = piecenumber;
  }

  ~nodeWithMatrix_c() {

    if (nodeMatrix[0])
      freeMatrix();
  }

  void freeMatrix(void) {

    assert(nodeMatrix[0]);

    for (int i = 0; i < 3; i++)
      delete [] nodeMatrix[i];

    nodeMatrix[0] = 0;
  }

  // this function returns true, if the given
  // piece has been moved in the last step
  // for the first node it always returns true
  // as everything needs to be calculated
  bool pieceMoved(int piece) {

    assert(piece < getPiecenumber());

    return ((getComefrom() == 0) ||
            (getComefrom()->getPiecenumber() != getPiecenumber()) ||
            (getX(piece) != getComefrom()->getX(piece)) ||
            (getY(piece) != getComefrom()->getY(piece)) ||
            (getZ(piece) != getComefrom()->getZ(piece)));
  }

  // returns the value in a place inside the matrix
  int getMatrixValue(int m, int p) {
    // matrix is not yet freed
    assert(nodeMatrix[0] != 0);
    assert(m < 3);
    assert(p < nodePiecenumber * nodePiecenumber);
    return nodeMatrix[m][p];
  }

};


/* so, this isn't the function as described by Bill but rather a
 * bit optimized. For each pair of 2 different pieces and for
 * each of the three dimensions I do the following:
 *  - check the intersection area area in this direction
 *  - if it's empty the pieces do not interlock and the matrix
 *    is initialized to infinity (32000)
 *  - if we have an intersection we check each column inside this area
 *    and find the shortest distance the the pirst piece follows
 *    the second and the second piece follows the first
 */
void disassembler_3_c::prepare(int pn, voxel_type * pieces, nodeWithMatrix_c * searchnode) {

  for (int l = 0; l < pn; l++)
    for (int d = 0; d < 3; d++)
      matrix[d][l + piecenumber * l] = 0;

  for (int i = 0; i < pn-1; i++)
    for (int j = i + 1; j < pn; j++)

      if (searchnode->pieceMoved(i) || searchnode->pieceMoved(j)) {

        // ok, one of the two pieces we want to calcuate with has moved since the last
        // iteration, that means we have to calculate the values anew
        voxel_type pi = pieces[i];
        voxel_type pj = pieces[j];
  
        /* calc the potential intersection area */
  
        /* c.. contains the bounding box bounding both pieces
         * d.. contains the intersection box of the bounding boxes of both pieces
         */
        int cx1, cx2, dx1, dx2, cy1, cy2, dy1, dy2, cz1, cz2, dz1, dz2;
  
        minmax(bx1[pi] + searchnode->getX(i), bx1[pj] + searchnode->getX(j), cx1, dx1);
        minmax(bx2[pi] + searchnode->getX(i), bx2[pj] + searchnode->getX(j), dx2, cx2);
  
        minmax(by1[pi] + searchnode->getY(i), by1[pj] + searchnode->getY(j), cy1, dy1);
        minmax(by2[pi] + searchnode->getY(i), by2[pj] + searchnode->getY(j), dy2, cy2);
  
        minmax(bz1[pi] + searchnode->getZ(i), bz1[pj] + searchnode->getZ(j), cz1, dz1);
        minmax(bz2[pi] + searchnode->getZ(i), bz2[pj] + searchnode->getZ(j), dz2, cz2);
  
        /* there are all in all 6 possible arrangements in the way the pieces can overlap
         * and for each possible arrangement another way is the most efficient to find the
         * smallest gap between 2 the 2 selected pieces
         * so the first thing is to find out in which arrangement the the bounding boxes are
         *
         * FIXME possible improvement: instead of using bounding boxes use some kind of array for each
         * voxel of the bounding box and calc how far the puzzle is away from the bounding box at this
         * position. This will enable further optimisations for a cost of a little more memory
         *
         * there are 6 possible arrangements:
         *
         * i 0   -- 1  -- 2  --  4 ---- 5 --  8 --
         * j   --     --    ----    --     --     --
         *
         * the numbering here is for programming reasons. it comes from the arrangement of the
         * start end ending points to one another. the missing cases are defunct.
         *
         * for each case we now write special code trating the not overlapping pieces more
         * efficient.
         *
         * all this code lowers the time to disassemble complex burrs to 2/3rds of the time the above
         * code takes
         */
  
        int arrangement, minimum, value;
        int facesizefactorI, facesizefactorJ;

        facesizefactorI = by2[pi]-by1[pi]+1;
        facesizefactorJ = by2[pj]-by1[pj]+1;
        {
          search(x, X, y, Y, z, Z, i, j, 4, facesizefactorI, facesizefactorJ);
          matrix[0][i + piecenumber * j] = minimum;
        } {
          search(x, X, y, Y, z, Z, j, i, 4, facesizefactorJ, facesizefactorI);
          matrix[0][j + piecenumber * i] = minimum;
        }

        facesizefactorI = bx2[pi]-bx1[pi]+1;
        facesizefactorJ = bx2[pj]-bx1[pj]+1;
        {
          search(y, Y, x, X, z, Z, i, j, 2, facesizefactorI, facesizefactorJ)
          matrix[1][i + piecenumber * j] = minimum;
        } {
          search(y, Y, x, X, z, Z, j, i, 2, facesizefactorJ, facesizefactorI);
          matrix[1][j + piecenumber * i] = minimum;
        }
        {
          search(z, Z, x, X, y, Y, i, j, 0, facesizefactorI, facesizefactorJ);
          matrix[2][i + piecenumber * j] = minimum;
        } {
          search(z, Z, x, X, y, Y, j, i, 0, facesizefactorJ, facesizefactorI);
          matrix[2][j + piecenumber * i] = minimum;
        }

      } else {
        // both pieces are still at the same place,
        // that means we can reuse the same value already
        // calculated in the last node

        nodeWithMatrix_c * n = (nodeWithMatrix_c *)searchnode->getComefrom();
        assert(n);

        matrix[0][i + piecenumber * j] = n->getMatrixValue(0, i + piecenumber * j);
        matrix[1][i + piecenumber * j] = n->getMatrixValue(1, i + piecenumber * j);
        matrix[2][i + piecenumber * j] = n->getMatrixValue(2, i + piecenumber * j);

        matrix[0][j + piecenumber * i] = n->getMatrixValue(0, j + piecenumber * i);
        matrix[1][j + piecenumber * i] = n->getMatrixValue(1, j + piecenumber * i);
        matrix[2][j + piecenumber * i] = n->getMatrixValue(2, j + piecenumber * i);
      }

  // save the surrent matrix for all the
  searchnode->setMatrix(matrix, piecenumber);

  /* second part of Bills algorithm. again a bit different from what he used to
   * describe it instead of repeatedly going over the array I recursively go down
   * and check the column and line of the modified field, if they do need adaption.
   * this prevents us from checking the whole array again if there was only one
   * cell changed the last time
   */

  bool again;

  do {

    again = false;

    for (int d = 0; d < 3; d++)
      for (int i = 0; i < pn; i++)
        for (int j = 0; j < pn; j++)
          for (int k = 0; k < pn; k++) {
            int l = matrix[d][i + piecenumber * j] + matrix[d][j + piecenumber * k];
            if (matrix[d][i + piecenumber * k] > l) {
              matrix[d][i + piecenumber * k] = l;
              again = true;
            }
          }

  } while (again);
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
 * to distinguish "good" and "bad" moves the function returns true, if less than halve of
 * the pieces need to be moved. This prevents the movement of the whole puzzle which
 * is rubbish.
 */
bool disassembler_3_c::checkmovement(void) {

  for (int i = 0; i < next_pn; i++)
    check[i] = movement[i] != 0;

  bool finished;

  /* we count the number of pieces that need to be moved, if this number
   * get's bigger than halve of the pices of the current problem we
   * stop and return that this movement is rubbish
   */
  int moved_pieces = 1;
  int nd = nextdir >> 1;

  if (nextdir & 1) {

    do {
  
      finished = true;
  
      for (int i = 0; i < next_pn; i++)
        if (check[i]) {
          for (int j = 0; j < next_pn; j++)
            if ((i != j) && (movement[i] > matrix[nd][j + piecenumber * i]))
              if (movement[i] - matrix[nd][j + piecenumber * i] > movement[j]) {
                if (movement[j] == 0) {
                  moved_pieces++;
                  if (moved_pieces > (next_pn / 2))
                    return false;
                }
                movement[j] = movement[i] - matrix[nd][j + piecenumber * i];
                check[j] = true;
                finished = false;
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
            if ((i != j) && (movement[i] > matrix[nd][i + piecenumber * j]))
              if (movement[i] - matrix[nd][i + piecenumber * j] > movement[j]) {
                if (movement[j] == 0) {
                  moved_pieces++;
                  if (moved_pieces > (next_pn / 2))
                    return false;
                }
                movement[j] = movement[i] - matrix[nd][i + piecenumber * j];
                check[j] = true;
                finished = false;
              }
          check[i] = false;
        }
  
    } while (!finished);
  }

  return true;
}

void disassembler_3_c::init_find(nodeWithMatrix_c * nd, int piecenumber, voxel_type * pieces) {

  /* when a new search has been started we need to first calculate
   * the movement matrixes, this is a table that contains one 2 dimensional
   * matrix for each of the 6 directions where movement is possible
   *
   * the matrixes contains possible movement of one piece if other pieces
   * are not moved. So a one in column 2 row 4 means that piece nr. 2 can
   * be moved one unit it we fix piece nr. 4
   *
   * the algorithm used here is describes in Bill Cuttlers booklet
   * "Computer Analysis of All 6 Piece Burrs"
   */
  prepare(piecenumber, pieces, nd);

  /* first we try to remove the piece completely by specifying
   * a very large distance, if it is possible to move this piece that
   * far, it is probably removed
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  next_pn = piecenumber;
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 *
 * FIXME: we should first try to remove a single piece, then to remove groups of pieces
 * and then check movement of pieces
 */
nodeWithMatrix_c * disassembler_3_c::find(nodeWithMatrix_c * searchnode) {

  while (nextdir < 6) {

    /* go through all directions */
    while (nextpiece < next_pn) {

      /* initialize the movement matrix. we want to move 'nextpiece' 'nextstep' units
       * into the current direction, so we initialize the matrix with all
       * zero except for our piece
       */
      for (int i = 0; i < next_pn; i++) movement[i] = 0;
      movement[nextpiece] = nextstep;

      /* checkmovement increases the values for all the other pieces so much that we can move
       * our selected piece as far as we want, if this results in more than halve of the
       * pieces beeing moved we don't do this because this would be stupid
       */
      if (checkmovement()) {

        nodeWithMatrix_c * n = new nodeWithMatrix_c(next_pn);

        /* create a new state with the pieces moved */
        for (int i = 0; i < next_pn; i++)
          switch(nextdir) {
          case 0: n->set(i, searchnode->getX(i) + movement[i], searchnode->getY(i), searchnode->getZ(i)); break;
          case 1: n->set(i, searchnode->getX(i) - movement[i], searchnode->getY(i), searchnode->getZ(i)); break;
          case 2: n->set(i, searchnode->getX(i), searchnode->getY(i) + movement[i], searchnode->getZ(i)); break;
          case 3: n->set(i, searchnode->getX(i), searchnode->getY(i) - movement[i], searchnode->getZ(i)); break;
          case 4: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) + movement[i]); break;
          case 5: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) - movement[i]); break;
          }

        if (nextstep == 1)
          nextstep = 30000;
        else
          nextstep ++;

        return n;
      }

      /* if we can not remove the piece we try to move it */
      if (nextstep == 30000)
        nextstep = 2;
      else {
        nextstep = 1;
        nextpiece++;
      }
    }

    nextpiece = 0;
    nextdir++;
  }

  return 0;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(nodeWithMatrix_c * st, nodeWithMatrix_c ** n, voxel_type ** pn, int piecenumber, voxel_type * pieces, int part, bool cond) {

  *n = new nodeWithMatrix_c(part);
  *pn = new voxel_type[part];

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond) {
      if (num == 0) {
        /* find the direction, the first piece was moved out of the puzzle
         * and shift it back along this avis */
        if ((st->getX(i) > 10000) || (st->getX(i) < -10000)) dx = st->getX(i);
        if ((st->getY(i) > 10000) || (st->getY(i) < -10000)) dy = st->getY(i);
        if ((st->getZ(i) > 10000) || (st->getZ(i) < -10000)) dz = st->getZ(i);
      }
      (*n)->set(num,
                st->getX(i) - dx,
                st->getY(i) - dy,
                st->getZ(i) - dz);
      (*pn)[num] = pieces[i];
      num++;
    }

  assert(num == part);
}

/* this is a bredth first search function that analyzes the movement of
 * an assembled problem. when the problem falls apart into 2 pieces the function
 * calls itself recursively. It returns null if the problem can not be taken apart
 * completely and otherwise the disassembly tree
 *
 * the parameters are:
 *  - piecenumber: the number of pieces of the current problem. because we can have
 *                 subproblems this number is not identical to the number of pieces
 *                 in the assembly voxel space
 *  - pieces: defines which pieces of the assemly voxel space are acually really present
 *            in the current subproblem
 *  - start: the start position of each piece
 *
 * the function takes over the ownership of the node and pieces. They are deleted at the end
 * of the function, so you must allocate them with new
 */
separation_c * disassembler_3_c::disassemble_rec(int piecenumber, voxel_type * pieces, nodeWithMatrix_c * start) {

  std::queue<nodeWithMatrix_c *> openlist;
  std::set<nodeWithMatrix_c *, node_ptr_less> closed;

  closed.insert(start);
  openlist.push(start);

  separation_c * erg = 0;

  /* while there are nodes left we should look at and we have not found a solution */
  while (!openlist.empty()) {

    /* remove the node from the open list and start examining */
    nodeWithMatrix_c * node = openlist.front();
    openlist.pop();

    init_find(node, piecenumber, pieces);

    nodeWithMatrix_c * st;

    while ((st = find(node))) {

      if (closed.find(st) != closed.end()) {

        /* the new node is already here. We have found a new longer way to that
         * node, so we can savely delete the new node and continue to the next
         */
        delete st;
        continue;
      }

      closed.insert(st);
      st->setComefrom(node);

      if (!st->is_separation()) {

        /* the new node is no solution so insert the
         * new state into the known state table
         * and the open list for later examination and go on to the next node
         */
        openlist.push(st);
        continue;
      }

      /* if we get here we have found a node that separated the puzzle into
       * 2 pieces. So we recoursively solve the subpuzzles and create a tree
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
      assert((part1 > 0) && (part2 > 0));

      separation_c * left, *remove;

      /* check each subproblem, if it's a problem */
      if (part1 > 1) {

        nodeWithMatrix_c *n;
        voxel_type * pn;
        create_new_params(st, &n, &pn, piecenumber, pieces, part1, false);
        remove = disassemble_rec(part1, pn, n);

      } else
        remove = 0;

      if (part2 > 1) {

        nodeWithMatrix_c *n;
        voxel_type * pn;
        create_new_params(st, &n, &pn, piecenumber, pieces, part2, true);
        left = disassemble_rec(part2, pn, n);

      } else
        left = 0;

      /* if poth subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if ((remove || (part1 == 1)) && (left || (part2 == 1))) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, piecenumber, pieces);

        do {
          state_c *s = new state_c(piecenumber);

          for (int i = 0; i < piecenumber; i++)
            s->set(i, st->getX(i), st->getY(i), st->getZ(i));

          erg->addstate(s);

          st = (nodeWithMatrix_c*)st->getComefrom();
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

      delete [] pieces;
    
      std::set<nodeWithMatrix_c *, node_ptr_less>::iterator it;
      for (it = closed.begin(); it != closed.end(); it++)
        delete *it;

      return erg;
    }

    /* we have checked all the successors of this node, so we don't need the matrix
     * any longer
     */
  }

  // free all the allocated nodes
  delete [] pieces;

  std::set<nodeWithMatrix_c *, node_ptr_less>::iterator i;
  for (i = closed.begin(); i != closed.end(); i++)
    delete *i;

  return 0;
}

disassembler_3_c::disassembler_3_c(const assembly_c * assembly, const puzzle_c * puz, unsigned int prob) : piecenumber(assembly->placementCount()) {

  /* allocate the necessary arrays */
  movement = new int[piecenumber];
  check = new bool[piecenumber];

  bx1 = new int[piecenumber];
  bx2 = new int[piecenumber];
  by1 = new int[piecenumber];
  by2 = new int[piecenumber];
  bz1 = new int[piecenumber];
  bz2 = new int[piecenumber];

  for (int j = 0; j < 3; j++)
    matrix[j] = new int[piecenumber * piecenumber];

  assm = assembly->getVoxelSpace(puz, prob, bx1, bx2, by1, by2, bz1, bz2);
}

disassembler_3_c::~disassembler_3_c() {
  delete [] movement;
  delete [] check;
  for (unsigned int k = 0; k < 3; k++)
    delete [] matrix[k];

  delete [] bx1;
  delete [] bx2;
  delete [] by1;
  delete [] by2;
  delete [] bz1;
  delete [] bz2;

  delete assm;
}


separation_c * disassembler_3_c::disassemble(void) {

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  nodeWithMatrix_c * start = new nodeWithMatrix_c(piecenumber);

  for (unsigned int i = 0; i < piecenumber; i++)
    start->set(i, 0, 0, 0);

  /* create pieces field. this field contains the
   * names of all present pieces. because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  voxel_type * pieces = new voxel_type[piecenumber];

  for (unsigned int j = 0; j < piecenumber; j++)
    pieces[j] = j;

  return disassemble_rec(piecenumber, pieces, start);
}

