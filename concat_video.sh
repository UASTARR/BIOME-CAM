#!/bin/sh

ffmpeg -f concat -safe 0 -i list.txt -c copy flight.mkv
