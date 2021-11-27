#!/bin/bash

PROJECTHOME=$(pwd)
sysOS=`uname -s`
LLVMHome="llvm-12.0.0.obj"
Z3Home="z3.obj"
install_path=`npm root --prefix ${HOME}`
export LLVM_DIR=$install_path/$LLVMHome
export Z3_DIR=$install_path/$Z3Home
export PATH=$LLVM_DIR/bin:$PATH
export PATH=$PROJECTHOME/bin:$PATH
if [[ $sysOS == "Darwin" ]]
then 
    export SVF_DIR=$install_path/SVF/
elif [[ $sysOS == "Linux" ]]
then 
    export SVF_DIR=$install_path/SVF/
fi 

echo "LLVM_DIR="$LLVM_DIR
echo "SVF_DIR="$SVF_DIR
echo "Z3_DIR="$Z3_DIR

echo "add the following lines to your rc files"

echo "export SVF_DIR=$install_path/SVF/"
echo "export LLVM_DIR=$install_path/$LLVMHome"
echo "export Z3_DIR=$install_path/$Z3Home"
echo "export PATH=$LLVM_DIR/bin:$PROJECTHOME/bin:$PATH"
