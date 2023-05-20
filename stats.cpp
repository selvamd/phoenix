#include <math.h>
#include <string.h>
#include <stdexcept>
#include "stats.hpp"

using namespace std;

void timedkscore::init(int meanLife)
{
    currdate.set();
    result = 0;
    AvgLifeDays = meanLife;
}

bool timedkscore::process(int today, double value)
{
	Timestamp nowdt;
	nowdt = std::to_string(today);
	if (currdate < nowdt) return false;    	
	//gives higher score to recent events
	result += value * exp( currdate.daysDiff(nowdt) * -1.0 / AvgLifeDays);
	return true;
}

void trend::init(int end, int start, int maxcnt)
{
	startdt = std::to_string(start);
	enddt = std::to_string(end);
    istartdt = startdt.getInternalDate();
    ienddt   = enddt.getInternalDate();

	maxcount = maxcnt;
	Ex = Ey = Exy = Ex2 = count = 0;
}

bool trend::process(int today, double value)
{
    if (today > ienddt) return false;
    if (today < istartdt) return false;
    if (maxcount > 0 && count > maxcount) return false;

	if (lastdt < today) 
	{
		lastdt = today;
		dlast = value;
	}

	//Timestamp nowdt;
	//nowdt = std::to_string(today);
	//nowdt.daysDiff(startdt); 
	auto days = dateToDays(today) - dateToDays(istartdt);
	Ex  += days;
	Ex2 += days*days;
	Ey  += value;
	Exy += value * days;
	count++;
	return true;
}

//change value per day
double trend::last() const 
{
	return dlast;
}

//change value per day
double trend::slope() const 
{ 
	if (count == 0) return 0; 
	return (count * Exy - Ex * Ey) / (count * Ex2 - Ex * Ex); 
}

//change value per day for $100 investment
double trend::momentum() const 
{
	if (count == 0) return 0; 
	return slope() * 200 / (yinit()+yend()); 
}

double trend::yinit() const 
{ 
	if (count == 0) return 0; 
	return (Ey * Ex2 - Ex * Exy) / (count * Ex2 - Ex * Ex); 	
}

double trend::yend()  const 
{ 
	if (count == 0) return 0; 
	return enddt.daysDiff(startdt) * slope() + yinit(); 		
}

p2_t::p2_t( )
{
	count = 0;

	add_end_markers( );
}

p2_t::~p2_t( )
{
    delete [] q;
    delete [] dn;
    delete [] np;
    delete [] n;
}

p2_t::p2_t( double quant )
{
	count = 0;

	add_end_markers( );

	add_quantile( quant );
}

void p2_t::add_end_markers( void )
{
	marker_count = 2;
	q = new double[ marker_count ];
	dn = new double[ marker_count ];
	np = new double[ marker_count ];
	n = new int[ marker_count ];
	dn[0] = 0.0;
	dn[1] = 1.0;

	update_markers( );
}

double * p2_t::allocate_markers( int count )
{
	double *newq = new double[ marker_count + count ];
	double *newdn = new double[ marker_count + count ];
	double *newnp = new double[ marker_count + count ];
	int *newn = new int[ marker_count + count ];

	memcpy( newq, q, sizeof(double) * marker_count );
	memcpy( newdn, dn, sizeof(double) * marker_count );
	memcpy( newnp, np, sizeof(double) * marker_count );
	memcpy( newn, n, sizeof(int) * marker_count );

	delete [] q;
	delete [] dn;
	delete [] np;
	delete [] n;

	q = newq;
	dn = newdn;
	np = newnp;
	n = newn;

	marker_count += count;

	return dn + marker_count - count;
}

void p2_t::update_markers( )
{
	p2_sort( dn, marker_count );

	/* Then entirely reset np markers, since the marker count changed */
	for( int i = 0; i < marker_count; i ++ ) {
		np[ i ] = (marker_count - 1) * dn[ i ] + 1;
	}
}

void p2_t::add_quantile( double quant )
{
	double *markers = allocate_markers( 3 );

	/* Add in appropriate dn markers */
	markers[0] = quant;
	markers[1] = quant/2.0;
	markers[2] = (1.0+quant)/2.0;

	update_markers( );
}

void p2_t::add_equal_spacing( int count )
{
	double *markers = allocate_markers( count - 1 );

	/* Add in appropriate dn markers */
	for( int i = 1; i < count; i ++ ) {
		markers[ i - 1 ] = 1.0 * i / count;
	}

	update_markers( );
}

inline int sign( double d )
{
	if( d >= 0.0 ) {
		return 1.0;
	} else {
		return -1.0;
	}
}

// Simple bubblesort, because bubblesort is efficient for small count, and
// count is likely to be small
void p2_t::p2_sort( double *q, int count )
{
	double k;
	int i, j;
	for( j = 1; j < count; j ++ ) {
		k = q[ j ];
		i = j - 1;
		
		while( i >= 0 && q[ i ] > k ) {
			q[ i + 1 ] = q[ i ];
			i --;
		}
		q[ i + 1 ] = k;
	}
}

double p2_t::parabolic( int i, int d )
{
	return q[ i ] + d / (double)(n[ i + 1 ] - n[ i - 1 ]) * ((n[ i ] - n[ i - 1 ] + d) * (q[ i + 1 ] - q[ i ] ) / (n[ i + 1] - n[ i ] ) + (n[ i + 1 ] - n[ i ] - d) * (q[ i ] - q[ i - 1 ]) / (n[ i ] - n[ i - 1 ]) );
}

double p2_t::linear( int i, int d )
{
	return q[ i ] + d * (q[ i + d ] - q[ i ] ) / (n[ i + d ] - n[ i ] );
}

void p2_t::add( double data )
{
	int i;
	int k = 0;
	double d;
	double newq;

	if( count >= marker_count ) {
		count ++;

		// B1
		if( data < q[0] ) {
			q[0] = data;
			k = 1;
		} else if( data >= q[marker_count - 1] ) {
			q[marker_count - 1] = data;
			k = marker_count - 1;
		} else {
			for( i = 1; i < marker_count; i ++ ) {
				if( data < q[ i ] ) {
					k = i;
					break;
				}
			}
		}

		// B2
		for( i = k; i < marker_count; i ++ ) {
			n[ i ] ++;
			np[ i ] = np[ i ] + dn[ i ];
		}
		for( i = 0; i < k; i ++ ) {
			np[ i ] = np[ i ] + dn[ i ];
		}

		// B3
		for( i = 1; i < marker_count - 1; i ++ ) {
			d = np[ i ] - n[ i ];
			if( (d >= 1.0 && n[ i + 1 ] - n[ i ] > 1)
			 || ( d <= -1.0 && n[ i - 1 ] - n[ i ] < -1.0)) {
				newq = parabolic( i, sign( d ) );
				if( q[ i - 1 ] < newq && newq < q[ i + 1 ] ) {
					q[ i ] = newq;
				} else {
					q[ i ] = linear( i, sign( d ) );
				}
				n[ i ] += sign(d);
			}
		}
	} else {
		q[ count ] = data;
		count ++;

		if( count == marker_count ) {
			// We have enough to start the algorithm, initialize
			p2_sort( q, marker_count );

			for( i = 0; i < marker_count; i ++ ) {
				n[ i ] = i + 1;
			}
		}
	}
}

double p2_t::result( )
{
	if( marker_count != 5 ) {
		throw std::runtime_error("Multiple quantiles in use");
	}
    return result( dn[(marker_count - 1) / 2] );
}

double p2_t::result( double quantile )
{
	if( count < marker_count ) {
		int closest = 1;
        p2_sort(q, count);
		for( int i = 2; i < count; i ++ ) {
			if( fabs(((double)i)/count - quantile) < fabs(((double)closest)/marker_count - quantile ) ) {
				closest = i;
			}
		}
		return q[ closest ];
	} else {
		// Figure out which quantile is the one we're looking for by nearest dn
		int closest = 1;
		for( int i = 2; i < marker_count -1; i ++ ) {
			if( fabs(dn[ i ] - quantile) < fabs(dn[ closest ] - quantile ) ) {
				closest = i;
			}
		}
		return q[ closest ];
	}
}

double series::movavg(int N, int start, int count)
{
    if (N < 0 || N > 2) return 0;

    int i = 0, n = 0;
    double res = 0;
    for (auto &val:values)
    {
        if (n++ >= start)
            res += (pow(++i,N) * val.second);
        if (i == count)
            break;
    }

    double sum = i;
    if (N == 1) sum = (i*i+i)/2;
    if (N == 2) sum = (2*i*i*i+3*i*i+i)/6;
    return res/sum;
}

double series::bumpiness(trend &t)
{
    double res = 0;
    for (auto &val:values)
    {
        auto days = dateToDays(val.first) - dateToDays(startdt);
        double yval = t.yinit() + t.slope()*days;
        res += pow(yval - val.second,2);
    }
    return pow(res/values.size(),0.5);
}

//calculates maxgain,maxloss during the period
double series::maxgain()
{
    double  minpx = 0, maxgain = 0, usedminpx = 0;

    for (auto &val:values)
    {
        if (minpx == 0 )
            usedminpx = minpx = val.second;

        if (val.second - minpx > maxgain)
        {
            maxgain = (val.second-minpx);
            usedminpx = minpx;
        }
        if (val.second < minpx) minpx = val.second;
    }
    return maxgain;
}

double series::maxloss()
{
    double  maxpx = 0, maxloss = 0, usedmaxpx = 0;

    for (auto &val:values)
    {
        if (maxpx == 0)
            usedmaxpx = maxpx = val.second;

        if (maxpx - val.second > maxloss)
        {
            maxloss = (maxpx-val.second);
            usedmaxpx = maxpx;
        }

        if (val.second > maxpx) maxpx = val.second;
    }
    return maxloss;
}

