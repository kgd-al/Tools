#!/usr/bin/awk -f

BEGIN {
  if (bins == "")  bins = 10;
  if (bins <= 0) {
    print "Invalid bin count provided:", bins, " <= 0"
    exit
  }
  bins = bins;
  if (lbase == "")  lbase = 10;
  if (lbase <= 0) {
    print "Invalid logarithmic base provided:", lbase, " <= 0"
    exit
  }
}

function min(a,b) { return a < b ? a : b; }
function max(a,b) { return a < b ? b : a; }
function floor(x) { return int(x+.5); }

{
  v = log($1) / log(lbase);
}

NR==1 {
  lmin = v;
  lmax = v;
}

FNR==NR {
  lmin = min(v, lmin);
  lmax = max(v, lmax);
  next
}

ENDFILE {
  if (FNR==NR) {
    binwidth = (lmax - lmin) / bins;
    lbinwidth = lbase^binwidth;
    print "[D] min:", lmin;
    print "[D] max:", lmax;
    printf "[D] binwidth = (%f - %f) / %f = %f\n", lmax, lmin, bins, binwidth;
  }
}

FNR<NR {
  i = floor((bins-1) * (v - lmin) / (lmax - lmin));
  b = (i + .5) * binwidth;
  print "[D]", $1, v, i, b, "in [", b-.5*binwidth, b+.5*binwidth, "]";
  binsArray[b]++
}

END {
  n=asorti(binsArray, indices)
  for (i=1; i<=n; i++) {
    b = indices[i]
    print "[D]", lbase^b, "in [", lbase^(b-.5*binwidth), ",", lbase^(b+.5*binwidth), "] mapsto ", b, "in [", b-.5*binwidth, ",", b+.5*binwidth, "]: ", binsArray[b];
    print lbase^b, binsArray[b], (lbase^(b+.5*binwidth) - lbase^(b-.5*binwidth))
  }
}
