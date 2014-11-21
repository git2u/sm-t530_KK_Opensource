#!/usr/bin/perl

if ($#ARGV != 1 ) {
	print "usage: perl Gen.pl INFILE OUTFILE\n";
	exit;
}
$file = $ARGV[0] || "List.txt";
$out = $ARGV[1] || "out.svg";
if (-e $file )
{
	printf "Input: $file Output:$out \n";
}
else
{
	print "Input File is not present\n";
	exit 0;
}
$command=`rm -rf Test_Final.txt`;
$command=`rm -rf Image_temp.txt`;
#$command=`cat $file | egrep \"MSCGEN|ERROR\" | grep \";\" > Image_temp.txt`;
$command=`cat $file | egrep \"MSCGEN|ERROR:\"  > Image_temp.txt`;

open I,"<Image_temp.txt";
open O,">Test_Final.txt";
print O " \n msc { \n width=\"1200\"; \n UI[label=\"UI\"],WC[label=\"WebContents\"],CC[label=\"Compositor\"],RVHM[label=\"RVHM\"],RWHVA[label=\"RWHVA\"],RVH[label=\"RVH\"],GPU[label=\"GPU\"],RENDERER[label=\"RENDERER\"]; \n \n";

while(<I>)
{
	$line=$_;
#	chomp($line);
	if (index($line, 'MSCGEN') != -1)
	{
		@dataArray=split('\^',$line);
		$data=$dataArray[1];
		@info=split(' ',$dataArray[0]);
		$timestamp=$info[1];
		$data=~ s/\] ;//g;
		$data=~ s/\];//g;
		$data=~ s///g;
		$data=~ s/\n//g;
		print O sprintf("%s,ID=\"%s\" ];\n",$data,$timestamp);
	}
	else
	{
		@dataArray=split('\]',$line);
		$data=$dataArray[1];
		@info=split(' ',$dataArray[0]);
		$timestamp=$info[1];
		$data=~ s///g;
		$data=~ s/\n//g;
		print O sprintf("---[label=\"%s\",ID=\"%s\" ];\n",$data,$timestamp);

	}
}
print O " 
}";
close O;
$command=`mscgen -T svg -o $out Test_Final.txt`;
