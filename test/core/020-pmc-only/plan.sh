#!/bin/bash

set -e

TEST_DIR=$PWD

# Figure out where Pegasus is installed
export PEGASUS_BIN_DIR=`pegasus-config --bin`
if [ "x$PEGASUS_BIN_DIR" = "x" ]; then
    echo "Please make sure pegasus-plan is in your path"
    exit 1
fi

echo "Generating the dax..."
export PYTHONPATH=`pegasus-config --python`
python daxgen.py dax.xml

cat > sites.xml <<END
<?xml version="1.0" encoding="UTF-8"?>
<sitecatalog xmlns="http://pegasus.isi.edu/schema/sitecatalog" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://pegasus.isi.edu/schema/sitecatalog http://pegasus.isi.edu/schema/sc-3.0.xsd" version="3.0">
    <site handle="local" arch="x86" os="LINUX">
        <head-fs>
            <scratch>
                <shared>
                    <file-server protocol="file" url="file://" mount-point="$TEST_DIR/exec"/>
                    <internal-mount-point mount-point="$TEST_DIR/exec"/>
                </shared>
            </scratch>
        </head-fs>
    </site>
</sitecatalog>
END

echo "Planning the workflow..."
pegasus-plan \
    --conf pegasusrc \
    --dir work \
    --dax dax.xml \
    --sites local

exit $?