#!/bin/sh

if [ $# -eq 0 ]
then
	action="status"
elif [ $# -eq 1 ]
then
	action=$1
else
	echo "Usage: $0 [action]"
	echo "       Will perform the requested git action (status by default) on all my dependant repositories"
fi

	
for f in Tools AgnosticPhylogenicTree PTreeCalibration
do
	cd $f

	echo
	echo "## $f ##"
	echo

	echo "executing \"git $action\""
	eval "git $action"

	cd -
	echo
done

