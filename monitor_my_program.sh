#!/bin/bash
# monitor_by_program.sh
# Versione semplificata che calcola e mostra solo le medie

LOG_FILE="program_resources.log"
echo "Monitoraggio risorse per programma avviato $(date)" > $LOG_FILE

# Avvia il processo principale
./bin/director &
MAIN_PID=$!
echo "Processo director avviato con PID $MAIN_PID" >> $LOG_FILE

# Attendi un momento per l'avvio di tutti i processi
sleep 2

# Inizializza contatori globali per le medie
ITERATIONS=0
DIRECTOR_CPU_TOTAL=0
DIRECTOR_MEM_TOTAL=0
DIRECTOR_VSZ_TOTAL=0
DIRECTOR_RSS_TOTAL=0
USER_CPU_TOTAL=0
USER_MEM_TOTAL=0
USER_VSZ_TOTAL=0
USER_RSS_TOTAL=0
WORKER_CPU_TOTAL=0
WORKER_MEM_TOTAL=0
WORKER_VSZ_TOTAL=0
WORKER_RSS_TOTAL=0
TICKET_CPU_TOTAL=0
TICKET_MEM_TOTAL=0
TICKET_VSZ_TOTAL=0
TICKET_RSS_TOTAL=0

# Monitora finché il processo principale è in esecuzione
while kill -0 $MAIN_PID 2>/dev/null; do
  ((ITERATIONS++))
  
  # Per director
  DIRECTOR_STATS=$(ps -C director -o pid,%cpu,%mem,vsz,rss --no-headers | awk '{cpu_sum+=$2; mem_sum+=$3; vsz_sum+=$4; rss_sum+=$5; count+=1} END {if(count>0) print cpu_sum, mem_sum, vsz_sum/count, rss_sum/count, count}')
  
  # Estrai valori per media
  if [ -n "$DIRECTOR_STATS" ]; then
    DIRECTOR_CPU=$(echo "$DIRECTOR_STATS" | awk '{print $1}')
    DIRECTOR_MEM=$(echo "$DIRECTOR_STATS" | awk '{print $2}')
    DIRECTOR_VSZ=$(echo "$DIRECTOR_STATS" | awk '{print $3}')
    DIRECTOR_RSS=$(echo "$DIRECTOR_STATS" | awk '{print $4}')
    # Verifica se i valori sono numeri validi prima di usarli
    if [[ "$DIRECTOR_CPU" =~ ^[0-9]*\.?[0-9]+$ ]]; then
      DIRECTOR_CPU_TOTAL=$(echo "$DIRECTOR_CPU_TOTAL + $DIRECTOR_CPU" | bc -l)
      DIRECTOR_MEM_TOTAL=$(echo "$DIRECTOR_MEM_TOTAL + $DIRECTOR_MEM" | bc -l)
      DIRECTOR_VSZ_TOTAL=$(echo "$DIRECTOR_VSZ_TOTAL + $DIRECTOR_VSZ" | bc -l)
      DIRECTOR_RSS_TOTAL=$(echo "$DIRECTOR_RSS_TOTAL + $DIRECTOR_RSS" | bc -l)
    fi
  fi
  
  # Per user
  USER_STATS=$(ps -C user -o pid,%cpu,%mem,vsz,rss --no-headers | awk '{cpu_sum+=$2; mem_sum+=$3; vsz_sum+=$4; rss_sum+=$5; count+=1} END {if(count>0) print cpu_sum, mem_sum, vsz_sum/count, rss_sum/count, count}')
  
  # Estrai valori per media
  if [ -n "$USER_STATS" ]; then
    USER_CPU=$(echo "$USER_STATS" | awk '{print $1}')
    USER_MEM=$(echo "$USER_STATS" | awk '{print $2}')
    USER_VSZ=$(echo "$USER_STATS" | awk '{print $3}')
    USER_RSS=$(echo "$USER_STATS" | awk '{print $4}')
    # Verifica se i valori sono numeri validi prima di usarli
    if [[ "$USER_CPU" =~ ^[0-9]*\.?[0-9]+$ ]]; then
      USER_CPU_TOTAL=$(echo "$USER_CPU_TOTAL + $USER_CPU" | bc -l)
      USER_MEM_TOTAL=$(echo "$USER_MEM_TOTAL + $USER_MEM" | bc -l)
      USER_VSZ_TOTAL=$(echo "$USER_VSZ_TOTAL + $USER_VSZ" | bc -l)
      USER_RSS_TOTAL=$(echo "$USER_RSS_TOTAL + $USER_RSS" | bc -l)
    fi
  fi
  
  # Per worker
  WORKER_STATS=$(ps -C worker -o pid,%cpu,%mem,vsz,rss --no-headers | awk '{cpu_sum+=$2; mem_sum+=$3; vsz_sum+=$4; rss_sum+=$5; count+=1} END {if(count>0) print cpu_sum, mem_sum, vsz_sum/count, rss_sum/count, count}')
  
  # Estrai valori per media
  if [ -n "$WORKER_STATS" ]; then
    WORKER_CPU=$(echo "$WORKER_STATS" | awk '{print $1}')
    WORKER_MEM=$(echo "$WORKER_STATS" | awk '{print $2}')
    WORKER_VSZ=$(echo "$WORKER_STATS" | awk '{print $3}')
    WORKER_RSS=$(echo "$WORKER_STATS" | awk '{print $4}')
    # Verifica se i valori sono numeri validi prima di usarli
    if [[ "$WORKER_CPU" =~ ^[0-9]*\.?[0-9]+$ ]]; then
      WORKER_CPU_TOTAL=$(echo "$WORKER_CPU_TOTAL + $WORKER_CPU" | bc -l)
      WORKER_MEM_TOTAL=$(echo "$WORKER_MEM_TOTAL + $WORKER_MEM" | bc -l)
      WORKER_VSZ_TOTAL=$(echo "$WORKER_VSZ_TOTAL + $WORKER_VSZ" | bc -l)
      WORKER_RSS_TOTAL=$(echo "$WORKER_RSS_TOTAL + $WORKER_RSS" | bc -l)
    fi
  fi
  
  # Per ticket_erogator
  TICKET_STATS=$(ps -C ticket_erogator -o pid,%cpu,%mem,vsz,rss --no-headers | awk '{cpu_sum+=$2; mem_sum+=$3; vsz_sum+=$4; rss_sum+=$5; count+=1} END {if(count>0) print cpu_sum, mem_sum, vsz_sum/count, rss_sum/count, count}')
  
  # Estrai valori per media
  if [ -n "$TICKET_STATS" ]; then
    TICKET_CPU=$(echo "$TICKET_STATS" | awk '{print $1}')
    TICKET_MEM=$(echo "$TICKET_STATS" | awk '{print $2}')
    TICKET_VSZ=$(echo "$TICKET_STATS" | awk '{print $3}')
    TICKET_RSS=$(echo "$TICKET_STATS" | awk '{print $4}')
    # Verifica se i valori sono numeri validi prima di usarli
    if [[ "$TICKET_CPU" =~ ^[0-9]*\.?[0-9]+$ ]]; then
      TICKET_CPU_TOTAL=$(echo "$TICKET_CPU_TOTAL + $TICKET_CPU" | bc -l)
      TICKET_MEM_TOTAL=$(echo "$TICKET_MEM_TOTAL + $TICKET_MEM" | bc -l)
      TICKET_VSZ_TOTAL=$(echo "$TICKET_VSZ_TOTAL + $TICKET_VSZ" | bc -l)
      TICKET_RSS_TOTAL=$(echo "$TICKET_RSS_TOTAL + $TICKET_RSS" | bc -l)
    fi
  fi
  
  # Tempo di attesa tra le misurazioni
  sleep 1
done

echo "Monitoraggio terminato $(date)" >> $LOG_FILE
echo "" >> $LOG_FILE
echo "========================================" >> $LOG_FILE
echo "STATISTICHE MEDIE COMPLESSIVE ($ITERATIONS campioni):" >> $LOG_FILE
echo "========================================" >> $LOG_FILE

# Calcola e stampa medie per director
echo "DIRECTOR (media):" >> $LOG_FILE
if [ "$ITERATIONS" -gt 0 ] && [ $(echo "$DIRECTOR_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
  DIRECTOR_CPU_AVG=$(echo "scale=2; $DIRECTOR_CPU_TOTAL / $ITERATIONS" | bc -l)
  DIRECTOR_MEM_AVG=$(echo "scale=2; $DIRECTOR_MEM_TOTAL / $ITERATIONS" | bc -l)
  DIRECTOR_VSZ_AVG=$(echo "scale=2; $DIRECTOR_VSZ_TOTAL / $ITERATIONS" | bc -l)
  DIRECTOR_RSS_AVG=$(echo "scale=2; $DIRECTOR_RSS_TOTAL / $ITERATIONS" | bc -l)
  # Aggiungi zero iniziale se manca
  [[ $DIRECTOR_CPU_AVG =~ ^\. ]] && DIRECTOR_CPU_AVG="0$DIRECTOR_CPU_AVG"
  [[ $DIRECTOR_MEM_AVG =~ ^\. ]] && DIRECTOR_MEM_AVG="0$DIRECTOR_MEM_AVG"
  [[ $DIRECTOR_VSZ_AVG =~ ^\. ]] && DIRECTOR_VSZ_AVG="0$DIRECTOR_VSZ_AVG"
  [[ $DIRECTOR_RSS_AVG =~ ^\. ]] && DIRECTOR_RSS_AVG="0$DIRECTOR_RSS_AVG"
  echo "  CPU: ${DIRECTOR_CPU_AVG}%, MEM: ${DIRECTOR_MEM_AVG}%, VSZ: ${DIRECTOR_VSZ_AVG} KB, RSS: ${DIRECTOR_RSS_AVG} KB" >> $LOG_FILE
else
  echo "  Nessun dato disponibile" >> $LOG_FILE
fi

# Calcola e stampa medie per user
echo "USER (media):" >> $LOG_FILE
if [ "$ITERATIONS" -gt 0 ] && [ $(echo "$USER_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
  USER_CPU_AVG=$(echo "scale=2; $USER_CPU_TOTAL / $ITERATIONS" | bc -l)
  USER_MEM_AVG=$(echo "scale=2; $USER_MEM_TOTAL / $ITERATIONS" | bc -l)
  USER_VSZ_AVG=$(echo "scale=2; $USER_VSZ_TOTAL / $ITERATIONS" | bc -l)
  USER_RSS_AVG=$(echo "scale=2; $USER_RSS_TOTAL / $ITERATIONS" | bc -l)
  # Aggiungi zero iniziale se manca
  [[ $USER_CPU_AVG =~ ^\. ]] && USER_CPU_AVG="0$USER_CPU_AVG"
  [[ $USER_MEM_AVG =~ ^\. ]] && USER_MEM_AVG="0$USER_MEM_AVG"
  [[ $USER_VSZ_AVG =~ ^\. ]] && USER_VSZ_AVG="0$USER_VSZ_AVG"
  [[ $USER_RSS_AVG =~ ^\. ]] && USER_RSS_AVG="0$USER_RSS_AVG"
  echo "  CPU: ${USER_CPU_AVG}%, MEM: ${USER_MEM_AVG}%, VSZ: ${USER_VSZ_AVG} KB, RSS: ${USER_RSS_AVG} KB" >> $LOG_FILE
else
  echo "  Nessun dato disponibile" >> $LOG_FILE
fi

# Calcola e stampa medie per worker
echo "WORKER (media):" >> $LOG_FILE
if [ "$ITERATIONS" -gt 0 ] && [ $(echo "$WORKER_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
  WORKER_CPU_AVG=$(echo "scale=2; $WORKER_CPU_TOTAL / $ITERATIONS" | bc -l)
  WORKER_MEM_AVG=$(echo "scale=2; $WORKER_MEM_TOTAL / $ITERATIONS" | bc -l)
  WORKER_VSZ_AVG=$(echo "scale=2; $WORKER_VSZ_TOTAL / $ITERATIONS" | bc -l)
  WORKER_RSS_AVG=$(echo "scale=2; $WORKER_RSS_TOTAL / $ITERATIONS" | bc -l)
  # Aggiungi zero iniziale se manca
  [[ $WORKER_CPU_AVG =~ ^\. ]] && WORKER_CPU_AVG="0$WORKER_CPU_AVG"
  [[ $WORKER_MEM_AVG =~ ^\. ]] && WORKER_MEM_AVG="0$WORKER_MEM_AVG"
  [[ $WORKER_VSZ_AVG =~ ^\. ]] && WORKER_VSZ_AVG="0$WORKER_VSZ_AVG"
  [[ $WORKER_RSS_AVG =~ ^\. ]] && WORKER_RSS_AVG="0$WORKER_RSS_AVG"
  echo "  CPU: ${WORKER_CPU_AVG}%, MEM: ${WORKER_MEM_AVG}%, VSZ: ${WORKER_VSZ_AVG} KB, RSS: ${WORKER_RSS_AVG} KB" >> $LOG_FILE
else
  echo "  Nessun dato disponibile" >> $LOG_FILE
fi

# Calcola e stampa medie per ticket_erogator
echo "TICKET_EROGATOR (media):" >> $LOG_FILE
if [ "$ITERATIONS" -gt 0 ] && [ $(echo "$TICKET_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
  TICKET_CPU_AVG=$(echo "scale=2; $TICKET_CPU_TOTAL / $ITERATIONS" | bc -l)
  TICKET_MEM_AVG=$(echo "scale=2; $TICKET_MEM_TOTAL / $ITERATIONS" | bc -l)
  TICKET_VSZ_AVG=$(echo "scale=2; $TICKET_VSZ_TOTAL / $ITERATIONS" | bc -l)
  TICKET_RSS_AVG=$(echo "scale=2; $TICKET_RSS_TOTAL / $ITERATIONS" | bc -l)
  # Aggiungi zero iniziale se manca
  [[ $TICKET_CPU_AVG =~ ^\. ]] && TICKET_CPU_AVG="0$TICKET_CPU_AVG"
  [[ $TICKET_MEM_AVG =~ ^\. ]] && TICKET_MEM_AVG="0$TICKET_MEM_AVG"
  [[ $TICKET_VSZ_AVG =~ ^\. ]] && TICKET_VSZ_AVG="0$TICKET_VSZ_AVG"
  [[ $TICKET_RSS_AVG =~ ^\. ]] && TICKET_RSS_AVG="0$TICKET_RSS_AVG"
  echo "  CPU: ${TICKET_CPU_AVG}%, MEM: ${TICKET_MEM_AVG}%, VSZ: ${TICKET_VSZ_AVG} KB, RSS: ${TICKET_RSS_AVG} KB" >> $LOG_FILE
else
  echo "  Nessun dato disponibile" >> $LOG_FILE
fi

# Media complessiva di tutti i processi
echo "TOTALE (media):" >> $LOG_FILE
if [ "$ITERATIONS" -gt 0 ]; then
  # Calcola la somma dei totali verificando che ci siano dati validi
  VALID_DATA=0
  CPU_SUM=0
  MEM_SUM=0
  
  if [ $(echo "$DIRECTOR_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
    CPU_SUM=$(echo "$CPU_SUM + $DIRECTOR_CPU_TOTAL" | bc -l)
    MEM_SUM=$(echo "$MEM_SUM + $DIRECTOR_MEM_TOTAL" | bc -l)
    VALID_DATA=1
  fi
  
  if [ $(echo "$USER_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
    CPU_SUM=$(echo "$CPU_SUM + $USER_CPU_TOTAL" | bc -l)
    MEM_SUM=$(echo "$MEM_SUM + $USER_MEM_TOTAL" | bc -l)
    VALID_DATA=1
  fi
  
  if [ $(echo "$WORKER_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
    CPU_SUM=$(echo "$CPU_SUM + $WORKER_CPU_TOTAL" | bc -l)
    MEM_SUM=$(echo "$MEM_SUM + $WORKER_MEM_TOTAL" | bc -l)
    VALID_DATA=1
  fi
  
  if [ $(echo "$TICKET_CPU_TOTAL > 0" | bc -l) -eq 1 ]; then
    CPU_SUM=$(echo "$CPU_SUM + $TICKET_CPU_TOTAL" | bc -l)
    MEM_SUM=$(echo "$MEM_SUM + $TICKET_MEM_TOTAL" | bc -l)
    VALID_DATA=1
  fi
  
  if [ "$VALID_DATA" -eq 1 ]; then
    TOTAL_CPU_AVG=$(echo "scale=2; $CPU_SUM / $ITERATIONS" | bc -l)
    TOTAL_MEM_AVG=$(echo "scale=2; $MEM_SUM / $ITERATIONS" | bc -l)
    # Aggiungi zero iniziale se manca
    [[ $TOTAL_CPU_AVG =~ ^\. ]] && TOTAL_CPU_AVG="0$TOTAL_CPU_AVG"
    [[ $TOTAL_MEM_AVG =~ ^\. ]] && TOTAL_MEM_AVG="0$TOTAL_MEM_AVG"
    echo "  CPU: ${TOTAL_CPU_AVG}%, MEM: ${TOTAL_MEM_AVG}%" >> $LOG_FILE
  else
    echo "  Nessun dato disponibile" >> $LOG_FILE
  fi
else
  echo "  Nessun dato disponibile" >> $LOG_FILE
fi