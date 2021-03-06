% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/fit_mpm.R
\name{fit_mpm}
\alias{fit_mpm}
\title{fit a a Move Persistence Model (mpm)}
\usage{
fit_mpm(x, model = c("mpm", "jmpm"), optim = c("nlminb", "optim"),
  verbose = 1, control = NULL, inner.control = NULL)
}
\arguments{
\item{x}{a data frame of observations (see details)}

\item{model}{mpm model to fit; either \code{mpm} with unpooled random walk variance parameters (\code{sigma_(g,i)}) or \code{jmpm} with a single, pooled random variance parameter (\code{sigma_g})}

\item{optim}{numerical optimizer}

\item{verbose}{report progress during minimization}

\item{control}{list of control parameters for the outer optimization (type ?nlminb or ?optim for details)}

\item{inner.control}{list of control parameters for the inner optimization}
}
\value{
a list with components
\item{\code{fitted}}{a dataframe of fitted locations}
\item{\code{par}}{model parameter summary}
\item{\code{data}}{input dataframe}
\item{\code{tmb}}{the tmb object}
\item{\code{opt}}{the object returned by the optimizer}
}
\description{
fit a random walk with time-varying move persistence to location data (e.g., output from \code{fit_ssm})
}
\examples{
## fit jmpm to two southern elephant seals
data(fssm)
dmp <- grab(fssm, "predicted", as_sf=FALSE)
dmp <- dmp[, c("id", "date", "lon", "lat")]
fmpm <- fit_mpm(dmp, model = "jmpm")


}
