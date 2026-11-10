#!/usr/bin/env python3
"""
Script per aggiungere colonna Compatibility a tutti i requisiti in REQUISITI_TRACCIABILITA.md
M1 Correzione 5
"""
import re
import sys

# Requisiti CONFLICTING identificati
CONFLICTING = {
    'R-CMD-03': 'CONFLICTING con R-CAL-01 (maxSOC 95% vs calibrazione 100%)',
    'R-CMD-06': 'CONFLICTING con CP4 Discharging mode (Phase 0 no discharge)',
    'R-CAL-01': 'CONFLICTING con R-CMD-03 (calibrazione 100% vs maxSOC 95%)',
    'R-PV-03': 'CONFLICTING con R-LOAD-04 (surplus usa carico stimato non misurato)',
    'R-SAF-02': 'CONFLICTING (risolto): Degraded actionable vs documentazione',
    'R-NRG-10': 'CONFLICTING (critico): sign flip battery power pending validazione M8',
}

def add_compatibility_to_requirement(req_block, req_id):
    """Aggiunge riga Compatibility dopo Status in un blocco requisito"""
    
    # Determina valore Compatibility
    if req_id in CONFLICTING:
        compat_value = f'**CONFLICTING** — {CONFLICTING[req_id]}'
    elif req_id in ['R-NRG-02', 'R-FOG-05']:
        compat_value = '**NOT_APPLICABLE** — Requisito opzionale/negativo'
    else:
        compat_value = '**ALIGNED** — Nessun conflitto noto'
    
    # Cerca la riga Status e aggiungi Compatibility dopo
    lines = req_block.split('\n')
    new_lines = []
    status_found = False
    
    for line in lines:
        new_lines.append(line)
        if '| **Status** |' in line and not status_found:
            # Aggiungi riga Compatibility
            new_lines.append(f'| **Compatibility** | {compat_value} |')
            status_found = True
    
    return '\n'.join(new_lines)

def process_file(filepath):
    """Processa il file aggiungendo Compatibility a tutti i requisiti"""
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern per trovare blocchi requisiti (### R-XXX-NN fino al prossimo ### o ---)
    pattern = r'(### (R-[A-Z]+-\d+)\n\|.*?\n(?:\|.*?\n)+)'
    
    def replace_requirement(match):
        req_block = match.group(1)
        req_id = match.group(2)
        
        # Verifica se Compatibility già presente
        if '| **Compatibility** |' in req_block:
            print(f'  {req_id}: Compatibility già presente, skip')
            return req_block
        
        print(f'  {req_id}: Aggiunta Compatibility')
        return add_compatibility_to_requirement(req_block, req_id)
    
    # Sostituisci tutti i requisiti
    new_content = re.sub(pattern, replace_requirement, content, flags=re.MULTILINE | re.DOTALL)
    
    # Scrivi file
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f'\nFile aggiornato: {filepath}')

if __name__ == '__main__':
    filepath = 'docs/REQUISITI_TRACCIABILITA.md'
    print(f'Aggiunta colonna Compatibility a tutti i requisiti in {filepath}')
    print()
    process_file(filepath)
    print('\nOperazione completata!')
