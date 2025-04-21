#!/bin/bash
#

declare -i PROVED=1
# parametr za ./bakc.sh
# 1 .. nemazu zdrojovy lubeman
$first_param=$1
#
# zazalohuj starou verzi lubemanu
# cislo verze je v lubeman/version
cd ~
# Test if the file does not exist
if [ ! -e "lubeman/version" ]; then
    echo "~lubeman/version neexistuje"
    name_tgz="lubeman_origo.tgz"	
else
    name_tgz="lubeman"$(<lubeman/version)".tgz"
fi
echo $name_tgz
if ((PROVED>0)); then
  cd ~/
  #name_tgz="lubeman"$(<lubeman/version)".tgz"
  echo $name_tgz
  tar --exclude='lubeman/myenv' \
      --exclude='lubeman/core/.*' \
      --exclude='lubeman/.*'  \
      -cvf - lubeman/ | gzip > $name_tgz
else
  echo "mock .. backup old lubeman "$name_tgz
fi

#kdyz budu kompilovat zdroj aktualniho klienta
#pridam za ./back.sh 1
#tim si vyrobim lubemanx.tgz, ktery budu odesilat na rpi
first_param=$1
#echo "first_param "$first_param
if ((first_param == 1)); then
  echo "first_param == 1"
  # udelej kopii pro rpi
  echo "cp "$name_tgz "./lubemanx.tgz"
  cp $name_tgz ./lubemanx.tgz
  $PROVED=0
fi

# vymaz starou zalohu
#echo "rm -rf lubeman.old"
if ((PROVED>0)); then
	rm -rf lubeman.old
else
	echo "mock .. rm -rf lubeman.old"
fi


# prehod lubeman do zalohy
#echo "mv lubeman lubeman.old"
if ((PROVED>0)); then
	mv lubeman lubeman.old
else
	echo "mocdk .. mv lubeman lubeman.old"
fi

# jmeno lubeman.tgz predane skriptu jako parametr
file_tgz=$1
if ((PROVED>0)); then
	tar xvfz $file_tgz
else
	echo "unpacking .. "$file_tgz
fi
#tar xvfz $file_tgz
#
#
