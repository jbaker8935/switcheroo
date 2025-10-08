# Puzzle Data Converter for F256 Switcharoo

This script converts JSON puzzle data to C source files for the F256 Switcharoo game.

Usage:
    python3 scripts/convert_puzzles.py output.c input1.json [input2.json ...]

The script reads puzzle data from a JSON file and generates a C source file with
compact binary representations suitable for embedded systems.

JSON Input Format:
```json
{
  "puzzles": [
    {
      "id": "puzzle_name",
      "swapRule": "classic|clears_own|swapped_clears|swapped_clears_own",
      "difficulty": 1-4,
      "startingPosition": [
        {"row": 1-8, "col": "A-D", "player": "A|B", "swapped": false|true}
      ],
      "exampleSolution": [
        {
          "player": "A|B",
          "move": {
            "from": {"row": 1-8, "col": "A-D"},
            "to": {"row": 1-8, "col": "A-D"},
            "type": "swap|empty",
            "priority": 0-7
          }
        }
      ]
    }
  ]
}
```

The generated C file will contain:
- Compact binary representations of piece positions and moves
- Puzzle collection structure for easy access
- Helper functions for puzzle management

Example:
    python3 scripts/convert_puzzles.py src/puzzle_data.c classic_d2.json classic_d3.json swapped_clears_d3.json
