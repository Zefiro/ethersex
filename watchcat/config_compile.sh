#!/bin/sh

ipv4() {
    echo "$1" | grep -q "^[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*$"
}
ipv6() {
    echo "$1" | egrep -q "^[0-9a-fA-F:]*$"
}
expand_ipv6() {
# ok ok, it isn't a oneliner anymore
  echo -n $1 | perl -pe '@a = split(":", $_); shift @a if ($a[0] eq ""); 
    $_ = ""; for $e (@a){ if ($e eq "") { $_ .= "0:" x (8 - $#a )} 
    else { $_ .= "$e:" }}' | sed 's/:$/\n/' 
}

test_path() {
  IFS=":"
  
  for check_path in $PATH; do  
     if [ -e "$check_path/$1" ]; then
       good="1"
     fi  
  done
  IFS=""

  if [ -z "$good" ]; then
    echo "Error: $1 not found in your path, can't continue"
    exit -1
  fi
}

test_path host

resolv() {
    ping -c1 $1 | sed 's/^.* (\([^\(]*\)).*$/\1/1;q'
}

grep -q "^#define IPV6_SUPPORT"  ../config.h; ipv6=$?
tmp=`mktemp`
tmp2=`mktemp`
echo "/* Autogenerated File */" 
echo "static struct EcmdSenderReaction ecmd_react[] PROGMEM = {" >> $tmp
while read line; do
  port=`echo $line | awk '{print $1}' | cut -c 2 | tr 'ABCDabcd' '01230123'`
  pin=`echo $line | awk '{print $1}' | cut -c 3`
  rising=`echo $line | awk '{print $2}'`
  host=`echo $line | awk '{print $3}'`
  commando=`echo $line |sed 's/^[^ ]* [^ ]* [^ ]* //' | sed 's/\"/\\\"/g'`
  # Host bearbeiten
  if ipv4 $host ; then
    # Skip entry if address is obviously an ipv4 address and the rape is
    # confgiured as ipv6
    if [ "x$ipv6" = "x0" ]; then
      continue;
    fi
    host=`echo $host | awk -F . '{print $1 " | (" $2 " << 8) , " $3 " | (" $4 " << 8)" }'`
  elif ipv6 $host; then
    # Skip entry if address is obviously an ipv4 address and the rape is
    # confgiured as ipv4
    if [ "x$ipv6" = "x1" ]; then
      continue;
    fi
    host=`echo $addr | awk '{print "0x" $5 }' | sed 's/:/, 0x/g'`
  else
    # probably an dns name, let's resolv it
    THOST=`host $host | while read addr; do
      if [ "x$ipv6" = "x0" ]; then
        if echo $addr | grep -q 'IPv6 address'; then
          echo $addr | awk '{print "0x" $5 }' | sed 's/:/, 0x/g'
        fi
      else
        if echo $addr | grep -q 'has address'; then
          echo $addr | awk '{print $4}' | awk -F . '{print "(" $1 " << 8) | "\
                $2 ", (" $3 " << 8) | " $4 }'
          break
        fi
      fi
    done`
    if [ -z "$THOST" ]; then
      echo "Couldn't resolv $host, continue" > /dev/stderr
      continue
    fi
    host="$THOST"
  fi
  textid=$RANDOM
  while grep -q $textid $tmp2; do
    textid=$RANDOM
  done
  echo $textid >> $tmp2
  echo "const char text$textid[] PROGMEM = \"$commando\";";
  echo "    {$port, $pin, $rising, $host, text$textid }, " >> $tmp
done < config

echo
cat $tmp
if [ "x$ipv6" = "x1" ]; then
  #ipv4
  echo   "    {255, 255, 255, 255, 255, NULL}"
else
  echo   "    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, NULL}"
fi
echo "};"

rm -f $tmp $tmp2