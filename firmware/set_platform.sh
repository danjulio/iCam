#!/bin/bash
#
# Build script for iCam and iCamMini.  Configures the build for the selected
# hardware.  Be sure the IDF has been configured in the top-level directory this
# script resides in.
#
# ./set_platform.sh [icam | icam_mini]
#
if [ "$1" = "icam" ]; then
	rm partitions.csv version.txt sdkconfig &> /dev/null
	rm -rf build &> /dev/null
	cp partitions_icam.csv partitions.csv
	cp version_icam.txt version.txt
	cp sdkconfig_icam sdkconfig
elif [ "$1" = "icam_mini" ]; then
	rm partitions.csv version.txt sdkconfig &> /dev/null
	rm -rf build &> /dev/null
	cp partitions_icam_mini.csv partitions.csv
	cp version_icam_mini.txt version.txt
	cp sdkconfig_icam_mini sdkconfig
else
	echo "Unknown argument: $1"
fi