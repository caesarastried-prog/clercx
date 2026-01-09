#!/usr/bin/env python3
"""
play.py — simple CLI to run engine selfplay or engine vs engine or human vs engine.
Features:
 - specify white and black engine paths
 - choose depth
 - support 'fastchess' mode (skip UCI handshake) for engines that accept direct position/go
 - limit number of half-moves for quick tests
"""

import subprocess
import sys
import time
import argparse

class Engine:
    def __init__(self, path, fastchess=False):
        self.path = path
        self.fastchess = fastchess
        try:
            self.process = subprocess.Popen(
                [path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                bufsize=1
            )
        except FileNotFoundError:
            raise

        # If engine speaks UCI, do handshake; if fastchess, skip it
        if not self.fastchess:
            self.send("uci")
            # wait for uciok
            while True:
                line = self.process.stdout.readline()
                if not line:
                    break
                if "uciok" in line:
                    break

    def send(self, command):
        try:
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()
        except BrokenPipeError:
            pass

    def get_bestmove(self, moves_str, depth=8):
        # Position command
        if moves_str:
            self.send(f"position startpos moves {moves_str}")
        else:
            self.send("position startpos")

        # Go command
        self.send(f"go depth {depth}")

        best_move = None
        while True:
            line = self.process.stdout.readline()
            if not line:
                break
            line = line.strip()
            # UCI engines will eventually send 'bestmove'
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    best_move = parts[1]
                else:
                    best_move = "(none)"
                break
            # Fast/simple engines might print just a move on stdout
            if self.fastchess and len(line) >= 4 and line[0].isalpha():
                # naive detection of move like e2e4
                token = line.split()[0]
                if len(token) >= 4 and token[0].isalpha() and token[1].isdigit():
                    best_move = token
                    break
        return best_move

    def quit(self):
        try:
            self.send("quit")
            self.process.wait(timeout=2)
        except Exception:
            try:
                self.process.kill()
            except Exception:
                pass


def board_init():
    # 8x8 board, rows 0..7 correspond to ranks 1..8
    board = [['.' for _ in range(8)] for _ in range(8)]
    pieces = list("RNBQKBNR")
    # White pieces rank 1
    for i, p in enumerate(pieces):
        board[0][i] = p.lower()  # white lowercase
    # White pawns rank2
    for i in range(8):
        board[1][i] = 'p'
    # Black pawns rank7
    for i in range(8):
        board[6][i] = 'P'
    # Black pieces rank8
    for i, p in enumerate(pieces):
        board[7][i] = p
    return board


def board_print(board):
    print("+------------------------+")
    for r in range(7, -1, -1):
        row = board[r]
        print(str(r+1) + "| ", end='')
        for c in range(8):
            print(row[c] + ' ', end='')
        print('|')
    print("+------------------------+")
    print('  a b c d e f g h')


def algebraic_to_coords(sq):
    file = ord(sq[0]) - ord('a')
    rank = int(sq[1]) - 1
    return rank, file


def apply_move(board, move_str, side, ep_square):
    # move_str like e2e4 or e7e8q
    promo = None
    if len(move_str) >= 5:
        promo = move_str[4]
    from_sq = move_str[0:2]
    to_sq = move_str[2:4]
    fr, fc = algebraic_to_coords(from_sq)
    tr, tc = algebraic_to_coords(to_sq)
    piece = board[fr][fc]
    # detect castling
    if piece.lower() == 'k' and abs(fc - tc) == 2:
        # king side
        if tc == 6:
            # move king
            board[tr][tc] = piece
            board[fr][fc] = '.'
            # move rook h->f
            board[tr][5] = board[tr][7]
            board[tr][7] = '.'
        else:
            board[tr][tc] = piece
            board[fr][fc] = '.'
            # rook a->d
            board[tr][3] = board[tr][0]
            board[tr][0] = '.'
        return board, None

    # en-passant capture
    if piece.lower() == 'p' and fc != tc and board[tr][tc] == '.' and ep_square is not None:
        # a diagonal capture to empty square -> en-passant
        # Remove pawn behind the to square depending on side
        if side == 'white':
            board[tr-1][tc] = '.'
        else:
            board[tr+1][tc] = '.'

    # normal capture or move
    board[tr][tc] = board[fr][fc]
    board[fr][fc] = '.'

    # promotion
    if promo:
        pchar = promo.lower()
        if side == 'black':
            # black pieces uppercase
            board[tr][tc] = pchar.upper()
        else:
            board[tr][tc] = pchar

    # set new ep_square if pawn double move
    new_ep = None
    if piece.lower() == 'p' and abs(tr - fr) == 2:
        # ep square is the square passed over
        if side == 'white':
            new_ep = (fr + 1, fc)
        else:
            new_ep = (fr - 1, fc)
    return board, new_ep


def play_self(white_engine, black_engine, depth, max_half_moves):
    moves = []
    board = board_init()
    ep_square = None
    side = 'white'
    print(f"Starting selfplay: {white_engine.path} (White) vs {black_engine.path} (Black)")
    print("--------------------------------------------------")
    board_print(board)

    for i in range(max_half_moves):
        turn = 'White' if i % 2 == 0 else 'Black'
        engine = white_engine if i % 2 == 0 else black_engine

        moves_str = ' '.join(moves)
        best = engine.get_bestmove(moves_str, depth=depth)

        if not best or best == '(none)':
            print(f"Game over. {turn} has no move (checkmate or stalemate or engine error).")
            break

        if i % 2 == 0:
            print(f"{i//2 + 1}. {best}")
        else:
            print(f"... {best}")

        # apply move to local board
        board, new_ep = apply_move(board, best, side, ep_square)
        ep_square = new_ep
        board_print(board)

        moves.append(best)
        side = 'black' if side == 'white' else 'white'

    print("\nGame moves:")
    print(' '.join(moves))

    return moves


def play_human_vs_engine(engine, depth, max_half_moves, human_is_white=True):
    moves = []
    print(f"Human vs Engine: {'Human(White)' if human_is_white else 'Human(Black)'}")
    print("Enter moves in UCI format like e2e4. Type 'quit' to stop.")

    for i in range(max_half_moves):
        human_turn = (i % 2 == 0) == human_is_white
        if human_turn:
            mv = input("Your move: ").strip()
            if mv == 'quit':
                break
            moves.append(mv)
            continue
        else:
            moves_str = ' '.join(moves)
            best = engine.get_bestmove(moves_str, depth=depth)
            if not best or best == '(none)':
                print("Engine has no move. Game over.")
                break
            print(f"Engine: {best}")
            moves.append(best)

    print("\nGame moves:")
    print(' '.join(moves))
    return moves


def main():
    parser = argparse.ArgumentParser(description='Play engines against each other or human.')
    parser.add_argument('--white', default='./clercx/clercx', help='Path to white engine (default ./clercx)')
    parser.add_argument('--black', default='./clercx/clercx', help='Path to black engine (default ./clercx)')
    parser.add_argument('--white-fastchess', action='store_true', help='Treat white engine as fastchess (no UCI handshake)')
    parser.add_argument('--black-fastchess', action='store_true', help='Treat black engine as fastchess (no UCI handshake)')
    parser.add_argument('--mode', choices=['selfplay', 'human-vs-engine', 'engine-vs-engine'], default='selfplay')
    parser.add_argument('--depth', type=int, default=8, help='Search depth to use for engine (default 8)')
    parser.add_argument('--max-half-moves', type=int, default=100, help='Limit of half-moves to play (default 100)')
    parser.add_argument('--human-white', action='store_true', help='When human-vs-engine, human plays white (default true)')

    args = parser.parse_args()

    try:
        white = Engine(args.white, fastchess=args.white_fastchess)
        black = Engine(args.black, fastchess=args.black_fastchess)
    except FileNotFoundError as e:
        print(f"Error launching engine: {e}")
        return

    try:
        if args.mode == 'selfplay':
            play_self(white, black, args.depth, args.max_half_moves)
        elif args.mode == 'engine-vs-engine':
            play_self(white, black, args.depth, args.max_half_moves)
        else:
            # human vs engine
            engine = black if args.human_white else white
            play_human_vs_engine(engine, args.depth, args.max_half_moves, human_is_white=args.human_white)
    finally:
        try:
            white.quit()
        except Exception:
            pass
        try:
            black.quit()
        except Exception:
            pass

if __name__ == '__main__':
    main()
