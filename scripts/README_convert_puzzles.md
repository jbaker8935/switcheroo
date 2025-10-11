# Puzzle Data Converter for F256 Switcharoo

This script converts JSON puzzle data into the fixed-record binary catalog used
by the F256 Switcharoo game.

Usage:
  python3 scripts/convert_puzzles.py output.bin input1.json [input2.json ...]

The script reads puzzle data from the provided JSON files and generates a binary
catalog whose layout matches the runtime deserialiser described in `design.md`.

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

The generated binary file contains:
- 16-bit puzzle count header
- Fixed-width puzzle records (identifier, swap rule, difficulty, solved flag)
- Normalised starting positions and packed example solutions

Example:
  python3 scripts/convert_puzzles.py assets/generated/puzzle_data.bin classic_d2.json classic_d3.json swapped_clears_d3.json
