#pragma once
#include <cmath>
#include "DataTypes.hpp"
#include <set>

//simple stats calculator, cannot be used for large datasets
class stats
{
     int enddt, startdt, maxcount;

     public:
        stats(int end = -1, int start = -1,int count = -1) : enddt(end), startdt(start), maxcount(count) { values.clear(); }

        void init(int end = -1, int start = -1,int count = -1)
        {
            enddt = end;startdt = start;maxcount = count;
            values.clear();
        } 

        //Only 0..count values are processed if count is set
        //Insertion order (small->large or large->small) doesnot matter.  
        void process( int dt, double data )
        {
            if (enddt > 0 && dt > enddt) return;
            if (startdt > 0 && dt < startdt) return;
            if (maxcount > 0 && (int)values.size() > maxcount) return;
            values.insert(data);
        }

        double min()    { return (size()==0)? 0:*(values.begin());  }
        double max()    { return (size()==0)? 0:*(values.rbegin()); }
        double median() { return percentile(0.5);    }

        double mean()
        {
            double sum = 0;
            for (auto d:values) sum += d;
            return sum/values.size();
        }

        double md()
        {
            double sum = 0, avg = mean();
            for (auto d:values) sum += fabs(d-avg);
            return sum/values.size();
        }

        double sd()
        {
            double Ex = 0, Ex2 = 0;
            for (auto d:values) 
            {
                Ex += d;
                Ex2 += d*d;
            }    
            return pow(Ex2/values.size() - (Ex*Ex)/(values.size()*values.size()), 0.5);
        }

        double xd()
        {
            double sum = 0, avg = mean();
            for (auto d:values) sum += pow(d-avg,4);
            return pow(sum/values.size(),0.25);
        }

        // Retrieve the value at a particular quantile.
        double percentile( double quantile )
        {
            double result = 0;
            int count = (int)(values.size() * quantile);
            int index = 0;
            for (auto d:values)
            {
                if (++index > count)
                    return d;
                result = d;
            }
            return result;
        }

        int size() { return (int)values.size(); }

     private:
        std::multiset<double> values;
};


//p2_t calc(0.5); (or use add_quantile to target multiple quantiles)
//calc.add(xxx) to add observations
//median = calc.result(); (or result(quantile) to print each quantile
class p2_t
{
     public:
        p2_t( );
        ~p2_t( );
        // Initialize a p^2 structure to target a particular quantile
        p2_t( double quantile );
        // Set a p^2 structure to target a particular quantile 
        // 0.0 < quant < 1.0
        void add_quantile( double quant );
        // Set a p^2 structure to target n equally spaced quantiles
        void add_equal_spacing( int n );
        // Call to add a data point into the structure
        void add( double data );
        // Retrieve the value of the quantile. This function may only be called
        //if only one quantile is targetted by the p2_t structure
        double result( );
        // Retrieve the value at a particular quantile.
        double result( double quantile );

     private:
        void add_end_markers( );
        double *allocate_markers( int count );
        void update_markers( );
        void p2_sort( double *q, int count );
        double parabolic( int i, int d );
        double linear( int i, int d );
        double *q;
        double *dn;
        double *np;
        int *n;
        int count;
        int marker_count;
};

class trend
{
	Timestamp startdt;
	Timestamp enddt;
    int istartdt;
    int ienddt;
    int maxcount;
    int count = 0, lastdt = 0;
    double Ex = 0, Ey = 0, Exy = 0, Ex2 = 0;
    double dlast;

    public:
        void init(int end, int start, int maxcnt = -1);

        //Only 0..count values are processed if count is set
        //Insertion order (small->large or large->small) doesnot matter.  
        bool process(int today, double value);

        //change value per day
        double slope() const;

        //change value per day for $100 investment
        double momentum() const; 

        //value at the start time
        double yinit() const;

        //value at the end time
        double yend() const;

        //value at the end time
        double last() const;

    friend std::ostream& operator <<(std::ostream& out, trend const &data)
    {
        out << std::setw(15) << data.startdt.getInternalDate()
            << std::setw(15) << data.enddt.getInternalDate()
            << std::setw(15) << data.slope()
            << std::setw(15) << data.momentum()
            << std::setw(15) << data.yinit()
            << std::setw(15) << data.yend();
        return out;
    }
};

class correlation
{
    double Ex2 = 0, Ey2 = 0, Ex = 0, Ey = 0, Exy = 0, N = 0;
    int enddt, startdt, maxcount;
    
    public:
        correlation(int end = -1, int start = -1,int count = -1) : enddt(end), startdt(start), maxcount(count)
        { Ex2 = Ey2 = Ex = Ey = Exy = N = 0; }

        void init(int end = -1, int start = -1,int count = -1)
        {
            enddt = end;startdt = start;maxcount = count;
            Ex2 = Ey2 = Ex = Ey = Exy = N = 0;
        }

        //Only 0..count values are processed if count is set
        //Insertion order (small->large or large->small) doesnot matter.
        void process( int dt, double x, double y )
        {
            if (enddt > 0 && dt > enddt) return;
            if (startdt > 0 && dt < startdt) return;
            if (maxcount > 0 && ((int)N) >= maxcount) return;
            N += 1.0;
            Ex2 += x*x;
            Ey2 += y*y;
            Ex  += x;
            Ey  += y;
            Exy += x*y;
            //std::cout << x << "," << y << std::endl;
        }

        double sdx() const
        {
            return pow(Ex2/N - (Ex*Ex)/(N*N), 0.5);    
        }

        double sdy() const
        {
            return pow(Ey2/N - (Ey*Ey)/(N*N), 0.5);    
        }
        
        //Correlation = NExy-Ex*Ey/sqrt((NEx2-ExEx)*(NEy2-EyEy))
        double eval() const
        {
            //std::cout << N << ",Exy=" << Exy << ",Ex2=" << Ex2 << ",Ey2=" << Ey2 << ",Ex=" << Ex << ",Ey=" << Ey << std::endl;
            return (N==0)? 0:(((N*Exy)-(Ex*Ey))/pow(((N*Ex2)-(Ex*Ex))*((N*Ey2)-(Ey*Ey)),0.5));
        }

    friend std::ostream& operator <<(std::ostream& out, correlation const &data)
    {
        out << std::setw(15) << data.startdt
            << std::setw(15) << data.enddt
            << std::setw(15) << data.N
            << std::setw(15) << data.eval();
        return out;
    }
};

class timedkscore
{
    Timestamp currdate;
    double result;
    int AvgLifeDays;

    public:
        void init(int MeanLife);
        bool process(int today, double value);
        double score() const { return result; }

    friend std::ostream& operator <<(std::ostream& out, timedkscore const &data)
    {
        out << std::setw(15) << data.currdate.getInternalDate()
            << std::setw(15) << data.AvgLifeDays
            << std::setw(15) << data.score();
        return out;
    }
};

//Uses decaying weights from newest to oldest observation when moving averaging a time series data
//for last x observations.
//The value of N controls the degree of decay between successive new-to-old values in a time series.
//N=0 ==> SMA (Simple moving avg) - No decay. Equally weights all observations
//N=1 ==> LMA (Linearly decayed moving avg) - Linear decay - Const decay after every observation.
//N=2,3,4 ==> EMA (Exponentially decayed moving avg) - Exponential decay after every observation.

//Calculated Weight i-th observation from current (i=0) is (x-i)^N/SumPow#N
//where SumPow#N = Sum of (i^N) where i=0..x.
//Below are direct formulae that can be used for N=0 to N=4
//SumPow#0 = x+1
//SumPow#1 = x^2/2 + x/2
//SumPow#2 = x^3/3 + x^2/2 + x/6
//SumPow#3 = x^4/4 + x^3/2 + x^2/4
//SumPow#4 = x^5/5 + x^4/2 + x^3/3 - x/30
//A generalized recursive equation for any N is as follows:
//      (N+1) * SumPow#N = (x+1)^(N+1) - (SumPow#0+SumPow#1..SumPow#N-1)

class series
{
    int enddt;
    int startdt;
    

    public:
        enum { SMA = 0, LMA = 1, EMA = 2 };
        series(int end = -1, int start = -1) : enddt(end), startdt(start) { values.clear(); }

    //Insertion order should be small->large date in timeseries.
    bool process(int dt, double data)
    {
        if (enddt > 0 && dt > enddt) return false;
        if (startdt > 0 && dt < startdt) return false;
        values.push_back(std::make_pair(dt,data));
        return true;
    }

    double movavg(int N, int start, int count);

    double bumpiness(trend &t);
    
    double maxgain();
    
    double maxloss();

    int size() { return (int)values.size(); }

    private:
        std::vector< std::pair<int,double> > values;
};
