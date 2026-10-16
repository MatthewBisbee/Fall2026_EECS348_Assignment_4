/*
EECS 348 Assignment 4
Sudoku solver: Ingest a 9x9 sudoku puzzle and return all solutions
Input: puzzle1.txt thru puzzle5.txt. Or dedicated filenames
Output: Filename, the original board, errors, and the possible solutions
Claude Opus 5.5
Matthew Bisbee
Created: 10/15/2026. Revised: 10/15/2026
Revision: Check output failures and exceptions; read tokens without a temporary list
*/

#include <array> // grid and used-digit masks
#include <exception> // handle standard exceptions
#include <fstream> // read puzzle files
#include <iostream> // print results and errors
#include <new> // handle allocation failure
#include <sstream> // split rows into tokens
#include <string> // handle strings
#include <vector> // store filename lists

// Store the 9x9 board; zero = blank
class SudokuGrid {
public:
    // Use nine rows and nine columns
    static constexpr int SIZE = 9;

    // Start with an empty board by filling each row with zeros
    SudokuGrid() {
        for (auto& row : cells_) {
            row.fill(0);
        }
    }

    // Return stored value at that location
    int get(int row, int col) const {
        return cells_[row][col];
    }

    // Store value at that location
    void set(int row, int col, int value) {
        cells_[row][col] = value;
    }

    // Print each row in order; underscores for blanks and digits for filled cells
    void print(std::ostream& out) const {
        for (int r = 0; r < SIZE; ++r) {
            for (int c = 0; c < SIZE; ++c) {
                if (c > 0) {
                    out << ' ';
                }
                int v = cells_[r][c];
                if (v == 0) {
                    out << '_';
                } else {
                    out << static_cast<char>('0' + v);
                }
            }
            out << '\n';
        }
    }

private:
    // Store the board values
    std::array<std::array<int, SIZE>, SIZE> cells_;
};

// Identify input errors separately from a valid puzzle
class PuzzleReader {
public:
    // Try to open the file, error if cannot
    // Only accept the board after all input checks pass
    bool read(const std::string& filename, SudokuGrid& grid, std::string& error) const {
        std::ifstream in(filename);
        if (!in) {
            error = "cannot open file '" + filename + "'";
            return false;
        }

        // Read into a fresh board; track accepted rows separately from file lines
        SudokuGrid parsed;
        std::string line;
        std::string token;
        int rowsRead = 0;
        int lineNumber = 0;

        // Read one line at a time and advance the line number
        // Remove a trailing carriage return if present
        while (std::getline(in, line)) {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Split the row into tokens; track count and first invalid entry
            std::istringstream tokens(line);
            int tokenCount = 0;
            int badColumn = 0;
            std::string badToken;

            // Reject rows past nine and store valid tokens directly in the temporary grid
            // Remember the first invalid token, but keep counting to check row length
            while (tokens >> token) {
                if (tokenCount == 0 && rowsRead == SudokuGrid::SIZE) {
                    error = "extra non-blank data at line " + std::to_string(lineNumber)
                            + " (more than 9 rows)";
                    return false;
                }
                if (tokenCount < SudokuGrid::SIZE && badColumn == 0) {
                    int value = 0;
                    if (tokenValue(token, value)) {
                        parsed.set(rowsRead, tokenCount, value);
                    } else {
                        badColumn = tokenCount + 1;
                        badToken = token;
                    }
                }
                ++tokenCount;
            }

            // Ignore blank lines; require nine tokens per row
            // Report wrong count before invalid tokens, then count accepted rows
            if (tokenCount == 0) {
                continue;
            }

            if (tokenCount != SudokuGrid::SIZE) {
                error = "line " + std::to_string(lineNumber) + " has "
                        + std::to_string(tokenCount) + " tokens; expected 9 ("
                        + (tokenCount < SudokuGrid::SIZE ? "too few" : "too many")
                        + ")";
                return false;
            }

            if (badColumn != 0) {
                error = "line " + std::to_string(lineNumber) + ", column "
                        + std::to_string(badColumn) + ": invalid token '" + badToken + "'";
                return false;
            }

            ++rowsRead;
        }

        // Check for read errors or missing rows, then accept the board
        if (in.bad()) {
            error = "I/O error while reading '" + filename + "'";
            return false;
        }

        if (rowsRead != SudokuGrid::SIZE) {
            error = "found " + std::to_string(rowsRead)
                    + " puzzle rows; expected 9";
            return false;
        }

        grid = parsed;
        return true;
    }

private:
    // Require one digit 1-9 or underscore
    // Store underscores as zero; return false for other tokens
    static bool tokenValue(const std::string& token, int& value) {
        if (token.size() != 1) {
            return false;
        }
        char ch = token[0];
        if (ch == '_') {
            value = 0;
            return true;
        }
        if (ch >= '1' && ch <= '9') {
            value = ch - '0';
            return true;
        }
        return false;
    }
};

// Backtrack through a copy and print solutions as found
class SudokuSolver {
public:
    // Copy the puzzle and reset solution count
    // Track used digits in each row, column and box; repeated clues are conflicts
    SudokuSolver(const SudokuGrid& puzzle, std::ostream& out)
        : work_(puzzle),
          out_(out),
          solutionCount_(0),
          cluesConsistent_(true) {
        rowUsed_.fill(0);
        colUsed_.fill(0);
        boxUsed_.fill(0);
        for (int r = 0; r < SudokuGrid::SIZE; ++r) {
            for (int c = 0; c < SudokuGrid::SIZE; ++c) {
                int d = work_.get(r, c);
                if (d == 0) {
                    continue;
                }
                int bit = 1 << (d - 1);
                int b = boxIndex(r, c);
                if ((rowUsed_[r] & bit) || (colUsed_[c] & bit) || (boxUsed_[b] & bit)) {
                    cluesConsistent_ = false;
                }
                rowUsed_[r] |= bit;
                colUsed_[c] |= bit;
                boxUsed_[b] |= bit;
            }
        }
    }

    // Search if clues do not conflict; return number of solutions
    long long solveAll() {
        if (cluesConsistent_) {
            search();
        }
        return solutionCount_;
    }

    // Return whether clues are consistent
    bool cluesConsistent() const {
        return cluesConsistent_;
    }

private:
    // Find which of the nine boxes contains this cell
    static int boxIndex(int r, int c) {
        return (r / 3) * 3 + (c / 3);
    }

    // Combine used digits, invert and keep nine bits
    // The remaining bits represent legal digits for this cell
    int candidates(int r, int c) const {
        int used = rowUsed_[r] | colUsed_[c] | boxUsed_[boxIndex(r, c)];
        return ~used & 0x1FF;
    }

    // Count choices by clearing one set bit at a time
    static int bitCount(int mask) {
        int count = 0;
        while (mask) {
            mask &= mask - 1;
            ++count;
        }
        return count;
    }

    // Find the empty cell with the fewest choices; start with none selected
    void search() {
        int bestRow = -1;
        int bestCol = -1;
        int bestMask = 0;
        int bestCount = 10;

        // Skip filled cells and track the empty cell with the fewest legal digits
        // Backtrack if any empty cell has no choices
        for (int r = 0; r < SudokuGrid::SIZE; ++r) {
            for (int c = 0; c < SudokuGrid::SIZE; ++c) {
                if (work_.get(r, c) != 0) {
                    continue;
                }
                int mask = candidates(r, c);
                int count = bitCount(mask);
                if (count < bestCount) {
                    bestRow = r;
                    bestCol = c;
                    bestMask = mask;
                    bestCount = count;
                    if (count == 0) {
                        return;
                    }
                }
            }
        }

        // No blanks means solved; count and print this board
        // Return to try other solutions
        if (bestRow == -1) {
            ++solutionCount_;
            out_ << "Solution " << solutionCount_ << ":\n";
            work_.print(out_);
            out_ << '\n';
            return;
        }

        // Try legal digits 1-9 in the chosen cell
        // Mark the digit used in its row, column and box, then recurse
        int b = boxIndex(bestRow, bestCol);
        for (int d = 1; d <= SudokuGrid::SIZE; ++d) {
            int bit = 1 << (d - 1);
            if (!(bestMask & bit)) {
                continue;
            }
            work_.set(bestRow, bestCol, d);
            rowUsed_[bestRow] |= bit;
            colUsed_[bestCol] |= bit;
            boxUsed_[b] |= bit;

            search();

            // Clear the digit from the masks and cell before the next attempt
            boxUsed_[b] &= ~bit;
            colUsed_[bestCol] &= ~bit;
            rowUsed_[bestRow] &= ~bit;
            work_.set(bestRow, bestCol, 0);
        }
    }

    // Store working board, output, used-digit masks, solution count and clue status
    SudokuGrid work_;
    std::ostream& out_;
    std::array<int, SudokuGrid::SIZE> rowUsed_;
    std::array<int, SudokuGrid::SIZE> colUsed_;
    std::array<int, SudokuGrid::SIZE> boxUsed_;
    long long solutionCount_;
    bool cluesConsistent_;
};

// Read, solve and print each file separately
class SudokuApplication {
public:
    // Exit codes for success, input errors and output errors
    static constexpr int EXIT_OK = 0;
    static constexpr int EXIT_INPUT_ERROR = 1;
    static constexpr int EXIT_OUTPUT_ERROR = 2;

    // Use given filenames, or puzzle1.txt thru puzzle5.txt by default
    SudokuApplication(int argc, char* argv[]) {
        if (argc > 1) {
            for (int i = 1; i < argc; ++i) {
                files_.push_back(argv[i]);
            }
        } else {
            for (int i = 1; i <= 5; ++i) {
                files_.push_back("puzzle" + std::to_string(i) + ".txt");
            }
        }
    }

    // Process files in order and remember input errors
    // Continue to later files unless output has failed
    int run() {
        bool anyInputError = false;
        for (std::size_t i = 0; i < files_.size(); ++i) {
            if (i > 0) {
                std::cout << '\n';
            }
            if (!processFile(files_[i])) {
                anyInputError = true;
            }
            if (!std::cout) {
                break;
            }
        }

        // Flush output and report a write error if failed
        // Otherwise return input error or success
        std::cout.flush();
        if (!std::cout) {
            std::cerr << "ERROR: writing to standard output failed; "
                         "results may be missing or incomplete\n";
            return EXIT_OUTPUT_ERROR;
        }
        return anyInputError ? EXIT_INPUT_ERROR : EXIT_OK;
    }

private:
    // Print filename and read into a fresh board
    // Report input errors separately from a valid puzzle with no solution
    bool processFile(const std::string& filename) {
        std::cout << "===== " << filename << " =====\n";

        SudokuGrid original;
        std::string error;
        if (!reader_.read(filename, original, error)) {
            std::cout << "ERROR: " << error << '\n';
            std::cerr << "ERROR (" << filename << "): " << error << '\n';
            return false;
        }

        // Print original puzzle, then solve a copy
        // Report conflicting clues, all solutions and their count, or no solution
        std::cout << "Original puzzle:\n";
        original.print(std::cout);
        std::cout << '\n';

        SudokuSolver solver(original, std::cout);
        if (!solver.cluesConsistent()) {
            std::cout << "Note: the given clues conflict (a digit repeats in a row, "
                         "column, or 3x3 box).\n";
        }
        long long count = solver.solveAll();

        if (count == 0) {
            std::cout << "No solution found\n";
        } else {
            std::cout << "Total solutions: " << count << '\n';
        }
        return true;
    }

    // Store filenames and file reader
    std::vector<std::string> files_;
    PuzzleReader reader_;
};

// Create the app and return its exit status
// Catch allocation or other exceptions, report the error and return 3
int main(int argc, char* argv[]) {
    constexpr int EXIT_EXCEPTION = 3;
    try {
        SudokuApplication app(argc, argv);
        return app.run();
    } catch (const std::bad_alloc&) {
        std::cerr << "FATAL: out of memory; processing stopped\n";
        return EXIT_EXCEPTION;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "; processing stopped\n";
        return EXIT_EXCEPTION;
    } catch (...) {
        std::cerr << "FATAL: unknown exception; processing stopped\n";
        return EXIT_EXCEPTION;
    }
}
