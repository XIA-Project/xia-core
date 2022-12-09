# libraries
library(colorspace) # for rainbow_hcl()
library(anytime) # for anytime(), anydate()

# global variables
WORKDIR = "/Users/rui/Desktop/work/git/xiav2-xia-core/arada/waveserveripudp/pdrlogs/"

plotPdrLog <- function(relInfname="pdr-log-all.txt", relOutfname="pdr-log-plot.pdf"){

	absInfname <- sprintf("%s/%s", WORKDIR, relInfname)
    absOutfname <- sprintf("%s/%s", WORKDIR, relOutfname)

	# columns are: macpair relnhops ecdf nsamples
	frame <- read.csv(absInfname, header=TRUE, sep=' ')

   	pdf(absOutfname, width=20, height=10)
   
    par(mar=c(4,4,2,1))
	par(cex=1.6, cex.axis=1.6, cex.lab=1.6)

	macpairs <- unique(frame$macpair)
	nmacpairs <- length(macpairs)
	colors <- rainbow_hcl(nmacpairs)

	i <- 1
	for (macpair_ in macpairs){
		subframe <- subset(frame, macpair == macpair_)

		if (i == 1){ # plot on the first 
			plot(subframe$pdr ~ subframe$tstamp, ylim=c(0, 1.1), type="o", xaxt="n", lwd=5, col=colors[i], lty=i, pch=i, ylab="", xlab="")
		
		datetimes <- anytime(frame$tstamp)
		axis.POSIXct(1, datetimes, format="%H:%M:%S")

		} else{
			lines(subframe$pdr ~ subframe$tstamp, type="o", lwd=5, col=colors[i], lty=i, pch=i)
		}
		i <- i+1;
	}
	
	grid(NULL, NULL, lty=6, col="darkgray")

	firstTstamp <- frame$tstamp[1]	
	xaxisLabel <- sprintf("Time of day (date: %s)", anydate(as.double(firstTstamp)));
	mtext(side=1, text=xaxisLabel, cex=2.24, outer=FALSE, line=2.5) # bottom
	mtext(side=2, text="Packet delivery ratio (PDR)", cex=2.24, outer=FALSE, line=2.5) # left

	# legend
	legend("bottomright", as.character(macpairs), col=colors, pch=rep(46,length(macpairs)), lty=c(1:nmacpairs), bg="white", lwd=4, cex=1.3)

	dev.off()
}

plotTest <- function(){
	
	 pdf("SampleGraph.pdf",width=7,height=5)
 x=rnorm(100)
 y=rnorm(100,5,1)
 plot(x,lty=2,lwd=2,col="red", ylim = c(min(x,y),max(x,y)))
 lines(y,lty=3,col="green")
 dev.off()
}
