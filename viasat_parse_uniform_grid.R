#### name        : viasat_parse_uniform_grid.R ####
#### date        : 2024-02-08 ####
#### author      : Rupert Lucas
#### description : process sinr results from uniform grid scenario

#### __________________________ ####
#### setup ####
library(jsonlite)
library(tibble)
library(dplyr)
library(tidyr)
library(stringr)
library(purrr)
library(ggplot2)
library(cowplot)

# get simulation results summary
# loads summary of sims (not the sim results themselves) and provides path to download the sim results
load_res_summary <- function(res_name, res_dir="~/inmarsat-iab-releases/") {
  res_path = paste0(res_dir, res_name, "/")
  if (!dir.exists(res_path)) stop(paste("Directory", res_path, "doesn't exist"))
  json_path = paste0(res_path, res_name, ".json")
  if (!file.exists(json_path)) stop(paste("File", json_path, "doesn't exist"))
  
  res_results <- fromJSON(json_path)
  res_results = res_results$results
  
  res_summary = tibble()
  res_summary$scriptName="";res_summary$simId="";res_summary$simPath=""
  
  for(i in 1:length(res_results)) {
    params=res_results[[i]]$params
    simId=res_results[[i]]$meta$id
    res = params %>% as_tibble()
    res$scriptName=res_name
    res$simId=simId
    res$simPath=paste0(res_path, "data/", simId)
    res_summary = bind_rows(res_summary, res)
  }
  res_summary=distinct(res_summary)
  
  if ("nodePosition" %in% colnames(res_summary)) {
    res_summary = res_summary %>% tidyr::separate(nodePosition, c("nodePositionConfig", "x", "y", "z"), sep = ",")
    res_summary = res_summary %>% mutate(dist_km = sqrt((as.numeric(x))^2 + (as.numeric(y))^2)/1000)
  }
  
  if ("nRowsCols" %in% colnames(res_summary)) {
    res_summary = res_summary %>% tidyr::separate(nRowsCols, c("nRows", "nCols"), sep = ",", remove = F)
    res_summary = res_summary %>% mutate(nRowsCols2=paste0(nRows, "rows/", nCols, "cols"))
  }
  
  return(res_summary)
}

# extract sinr from individual simulation
extract_sinr <- function(sim_path, res_summary) {
  rx_packet_trace_colnames = c('direction', 'time', 'frame', 'subf', 'slot',  'firstSim', 'simNum', 'cid', 'rnti', 'ccId', 'tb', 'mcs', 'rv', 'sinr', 'corr', 'tbler')
  sim_id=stringr::str_split(sim_path, "/", simplify = T)
  sim_id=sim_id[length(sim_id)] # extract only last element
  if (!file.exists(paste0(sim_path, "/RxPacketTrace.txt"))) {
    data=tibble(direction="", simId = sim_id)
    data=data %>% left_join(res_summary, by="simId")
    logdebug("Sim not found %s, %s, %s", data$nodePositionConfig, data$nRowsCols2, data$seaStateCode)
    return(data)
  }
  data = read.csv(paste0(sim_path, "/RxPacketTrace.txt"),  header = T, sep = '\t', col.names = rx_packet_trace_colnames) %>% as_tibble()
  data$simId=sim_id
  data = data %>% left_join(res_summary, by="simId")
  return(data) 
}

#### __________________________ ####

# results ####

# parse scenario
res_summary_viasat_uniform_grid=load_res_summary(res_name = "results_viasat_uniform_grid", res_dir = "~/inmarsat-iab-releases/")

# * sinr ####
res_sinr_viasat_uniform_grid=purrr::map_df(res_summary_viasat_uniform_grid$simPath, extract_sinr, res_summary=res_summary_viasat_uniform_grid) 

# plot sinr
res_sinr_viasat_uniform_grid %>%
  filter(direction=="DL", nRowsCols=="8,8") %>%
  mutate(x=as.numeric(x), y=as.numeric(y)) %>%
  group_by(x,y) %>% 
  summarise(sinr=median(sinr, na.rm=T), .groups = "drop") %>%
  ggplot(aes(x, y, fill=sinr)) + 
  theme_cowplot(12) + 
  theme(legend.position = "right", legend.key.height = unit(1.75, "cm"), axis.text.x = element_text(angle = 90, vjust = 0.5, hjust=1)) +
  geom_tile() +
  scale_fill_gradient2("SINR\n[dB]", breaks=seq(-50,50,5)) +
  coord_cartesian(ylim = c(-6250,6250), xlim = c(-6250,6250), expand = F) +
  scale_y_continuous("y [m]", breaks = seq(-6000,6000,500)) +
  scale_x_continuous("x [m]", breaks = seq(-6000,6000,500))
