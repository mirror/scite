#!/bin/bash
# @file PullSource.sh
# Released to the public domain.

# Script to update Lexilla, Scintilla, and SciTE from remote repository.
# May fail for more complex local states.

(

cd $(dirname $0)/../..

(
cd lexilla || exit
git pull --rebase
)

hg pull -u -R scintilla
hg pull -u -R scite

)
