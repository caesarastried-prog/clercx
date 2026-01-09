#!/bin/bash
ENGINE="./clercx"
for depth in {8..12}
do
    echo "Testing depth $depth..."
    echo -e "position startpos\ngo depth $depth\nquit" | $ENGINE | grep "info depth $depth "
done
