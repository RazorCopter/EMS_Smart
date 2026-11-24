/* Obsidian Energy: external profile + settings UI. No third-party runtime. */
(() => {
  const VERSION='20261024-5';
  const clone = value => JSON.parse(JSON.stringify(value));
  const escapeHTML = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const finite = value => value !== '' && value !== null && Number.isFinite(Number(value));
  const unavailable = value => value == null || ['unknown','unavailable','none',''].includes(value);
  const owners = new Map();
  const palette = {red:'#ef7b87',orange:'#e9a064',yellow:'#e8c46b',green:'#7dd9ba'};
  let sequence = 0;

  function validate(p) {
    if (p?.version !== 1 || !p.dashboard?.card || !p.params || !p.entities || !Array.isArray(p.appliances) || !Array.isArray(p.actions)) throw new Error('Profilo non valido o versione non supportata.');
    for (const [key,field] of Object.entries(p.params)) {
      if (field.type === 'number' && (!finite(field.value) || (field.min != null && Number(field.value)<field.min) || (field.max != null && Number(field.value)>field.max))) throw new Error('Valore fuori intervallo: '+field.label+' ('+key+').');
      if (field.type === 'number') field.value=Number(field.value);
    }
    for (const [old,value] of Object.entries(p.entities)) if (!/^[a-z_]+\.[a-zA-Z0-9_]+$/.test(value)) throw new Error('Entita non valida: '+old);
    for (const color of Object.values(p.colors)) if (!/^#[0-9a-f]{6}$/i.test(color.value)) throw new Error('Colore non valido: '+color.label);
    for (const color of Object.values(p.applianceColors||palette)) if (!/^#[0-9a-f]{6}$/i.test(color)) throw new Error('Colore utenze non valido.');
    if (!Number.isInteger(p.params.applianceColumns.value)) throw new Error('Il numero di colonne deve essere intero.');
    for (const a of p.appliances) {
      if (!/^[a-z_]+\.[a-zA-Z0-9_]+$/.test(a.entity) || !a.name || !a.icon?.startsWith('mdi:') || a.thresholds.length !== 3) throw new Error('Utenza non valida: '+a.name);
      if (a.thresholds.some(([v,c])=>!finite(v)||!palette[c])) throw new Error('Soglie non valide: '+a.name);
    }
    for (const a of p.actions) {
      if (!/^[a-z_]+\.[a-zA-Z0-9_]+$/.test(a.entity) || !/^[a-z_]+\.[a-zA-Z0-9_]+$/.test(a.service) || !a.confirm?.trim()) throw new Error('Comando o conferma non validi: '+a.name);
      if (![a.x,a.y].every(v=>finite(v)&&v>=0&&v<=100)) throw new Error('Posizione non valida: '+a.name);
    }
    for (const f of Object.values(p.photo)) if (!/^[MmLlHhVvCcSsQqTtAaZz\d\s.,+eE-]+$/.test(f.path)) throw new Error('Percorso SVG non valido.');
    for (const value of Object.values(p.images)) if (/[<>"']/.test(value)) throw new Error('Percorso immagine non valido.');
    if (!p.images.base.endsWith('/')) p.images.base+='/';
    for (const key of ['forecastStart','forecastEnd']) if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(p.params[key].value)) throw new Error('Orario non valido: '+key);
    for (const key of ['monthConsumption','monthProduction','monthImport','monthExport']) if (!p.params[key].value.includes('{year}') || !p.params[key].value.includes('{month}')) throw new Error('Il modello mensile deve contenere {year} e {month}: '+key);
    return p;
  }

  function materialize(input, owner, ui = {}) {
    const p=validate(clone(input)), v=key=>p.params[key].value;
    const ids=Object.keys(p.entities).sort((a,b)=>b.length-a.length);
    const re=new RegExp('\\b('+ids.map(x=>x.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')).join('|')+')(?![a-zA-Z0-9_])','g');
    const bind = s => s.replace(re, id=>p.entities[id]);
    const colorRe=new RegExp(Object.keys(p.colors).join('|'),'gi');
    const runtime={params:p.params,entities:p.entities,images:p.images,weatherRules:p.weatherRules};
    const prefix=`\nconst OB=${JSON.stringify(runtime)};\nconst opt=(key)=>{const f=OB.params[key];if(f.helper&&f.source!=='local'){const id=OB.entities[f.helper]||f.helper;const raw=states[id]?.state;if(raw!==undefined&&!['unknown','unavailable','none',''].includes(raw)&&Number.isFinite(Number(raw)))return Number(raw);}return f.value;};\nconst escapeHTML=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));\nconst monthEntity=(key,y,m)=>String(opt(key)).replace('{year}',y).replace('{month}',m);\nconst timeParts=s=>String(s).split(':').map(Number);\n`;
    function walk(x) {
      if (Array.isArray(x)) return x.map(walk);
      if (x && typeof x==='object') return Object.fromEntries(Object.entries(x).map(([k,y])=>[k,walk(y)]));
      if (typeof x!=='string') return x;
      let text=bind(x).replaceAll('__ENERGY_OWNER__',owner);
      text=text.replace(colorRe,old=>p.colors[old.toLowerCase()]?.value||old);
      if (text.trim().startsWith('[[[') && (/\bopt\(|\bOB\.|\bmonthEntity\(|\btimeParts\(/.test(text))) text=text.replace(/^(\s*\[{3,})/, '$1'+prefix);
      return text;
    }
    const dashboard=walk(p.dashboard), root=dashboard.card, hero=root.cards[0], charts=root.cards[1], bottom=root.cards[2];
    const appliancesOpen=ui.appliancesOpen===true;
    const consumptionHeader=charts.cards[2];
    const toggleButton=`<button type="button" class="consumption-toggle" data-obsidian-ui="appliances" data-obsidian-owner="${owner}" aria-expanded="${appliancesOpen}" title="${appliancesOpen?'Nascondi':'Mostra'} consumi per utenza">Consumo casa <span aria-hidden="true">${appliancesOpen?'▴':'▾'}</span></button>`;
    consumptionHeader.custom_fields.content=consumptionHeader.custom_fields.content.replace(/Consumo Casa/i,toggleButton);
    // The real button handles clicks and keyboard activation once. Keeping
    // the surrounding card inert avoids a second HA tap action on the same click.
    consumptionHeader.tap_action={action:'none'};
    consumptionHeader.tooltip=appliancesOpen?'Nascondi consumi per utenza':'Mostra consumi per utenza';
    consumptionHeader.extra_styles+='\n.consumption-toggle{display:inline-flex;align-items:center;gap:7px;background:transparent;border:0;padding:4px 0;min-height:32px;color:inherit;font:inherit;cursor:pointer;touch-action:manipulation;pointer-events:auto!important}.consumption-toggle span{color:#8abde5;font-size:12px;pointer-events:none}.consumption-toggle:focus-visible{outline:2px solid #7dd9ba;outline-offset:4px;border-radius:3px}\n';
    const monthOpen=ui.monthOpen===true;
    let bottomContent=bottom.custom_fields.content;
    bottomContent=bottomContent.replace(/if \(typeof window\._pvBotTab === 'undefined'\) window\._pvBotTab = 'oggi';\s*const isMese = window\._pvBotTab === 'mese';/,`const isMese = ${monthOpen};`);
    bottomContent=bottomContent.replace(/<button class="bc-tab \$\{!isMese\?'active':''\}" onclick="[^"]*">Oggi & Solcast<\/button>/,`<button type="button" class="bc-tab \${!isMese?'active':''}" data-obsidian-ui="month-today" data-obsidian-owner="${owner}" aria-pressed="\${!isMese}">Oggi & Solcast</button>`);
    bottomContent=bottomContent.replace(/<button class="bc-tab \$\{isMese\?'active':''\}" onclick="[^"]*">Riepilogo Mese<\/button>/,`<button type="button" class="bc-tab \${isMese?'active':''}" data-obsidian-ui="month-summary" data-obsidian-owner="${owner}" aria-pressed="\${isMese}">Riepilogo Mese</button>`);
    bottom.custom_fields.content=bottomContent;
    bottom.extra_styles+='\n.bc-tab{touch-action:manipulation;pointer-events:auto!important}\n';
    root.layout['grid-gap']=v('gap')+'px';
    const query=root.layout.mediaquery['(min-width: 1101px) and (min-height: 760px)'];
    const total=v('desktopOffset')+v('gap'), share=v('bottomShare'), minimum=v('bottomMin'), header=v('headerHeight');
    query.height=`calc(100dvh - ${v('desktopOffset')}px)`;
    query['grid-template-rows']=`minmax(0,1fr) max(${minimum}px,calc((100dvh - ${total}px) * ${share}))`;
    query['grid-template-columns']=`minmax(0,min(60%,calc((min((100dvh - ${total}px) * ${1-share},100dvh - ${total+minimum}px) - ${header}px)*1.77778))) minmax(0,1fr)`;
    // Geometry is external; both the visible stroke and animation use one path.
    let content=hero.custom_fields.content;
    const originals={solar:['M205 99 H290 L390 150 H490',490,150],home:['M205 285 H270 L340 265 H425',425,265],battery:['M810 326 H775 L706 355',706,355],grid:['M480 456 L440 430 L394 384',394,384]};
    for (const [key,[path,x,y]] of Object.entries(originals)) {
      const f=p.photo[key]; content=content.replaceAll(path,f.path).replaceAll(`cx="${x}" cy="${y}"`,`cx="${f.anchorX}" cy="${f.anchorY}"`);
    }
    content=content.replace(/(flow\('battery',[^\n]+?bat,\s*)30/, '$1opt("batteryDeadband")').replace(/(flow\('grid',[^\n]+?grid,\s*)20/, '$1opt("gridDeadband")');
    content=content.replace("const reverse = id === 'battery' ? power < 0 : power > 0;","const reverse = id === 'battery' ? power > 0 : power < 0;");
    content=content.replace(/<button\b[^>]*class="hud-cfg-btn"[^>]*>/g,tag=>tag.replace(/\s+onclick="[^"]*"/g,'').replace(/>$/,` data-obsidian-ui="settings" data-obsidian-owner="${owner}">`));
    hero.custom_fields.content=content;
    const size=(min,vw,max,scale)=>`clamp(${min*scale}px,${vw*scale}vw,${max*scale}px)`;
    hero.extra_styles+=`\n.hero-topline{height:${header}px!important;min-height:${header}px!important;max-height:${header}px!important}.hero-brand{font-size:${size(22,1.25,32,v('titleScale'))}!important}.photo-stage>img{filter:saturate(${v('photoSaturation')}) brightness(${v('photoBrightness')})!important}.photo-stage .scene-summary{background:rgba(27,32,37,${v('summaryOpacity')})!important}.photo-stage .live-metric strong{font-size:clamp(${27*v('valueScale')}px,${3.6*v('valueScale')}cqw,${43*v('valueScale')}px)!important}\n`;
    for (const [key,f] of Object.entries(p.photo)) {
      const cls={solar:'solar',home:'home',battery:'battery',grid:'grid'}[key];
      hero.extra_styles+=`.photo-stage .metric-${cls}{${['left','right','top','bottom'].filter(k=>f[k]!=null).map(k=>`${k}:${Number(f[k])}%`).join(';')}}\n`;
    }
    for (const card of [hero,bottom,...charts.cards.filter(c=>c.type==='custom:button-card')]) card.update_timer=v('refreshSeconds')+'s';
    bottom.extra_styles+=`\n.kpi-val{font-size:${size(19,1.1,30,v('detailScale'))}!important}.bc-val{font-size:${size(27,1.6,41,v('valueScale'))}!important}.kpi-sub{font-size:${size(10,.66,17,v('detailScale'))}!important}\n`;
    charts.layout['grid-template-rows']=`auto minmax(${v('graphHeight')}px,1fr) auto minmax(${v('graphHeight')}px,1fr) auto`;
    for (const card of charts.cards) if (card.type==='custom:apexcharts-card') {
      card.graph_span=v('graphHours')+'h';
      card.card_mod.style=card.card_mod.style.replace(/min-height:\s*70px/g,`min-height:${v('graphHeight')}px`);
      for(const series of card.series){series.fill_raw='null';series.extend_to=false;}
      const chargeLimit=card.series.find(series=>series.yaxis_id==='amp');if(chargeLimit)chargeLimit.curve='stepline';
      const amp=card.yaxis?.find(a=>a.id==='amp'); if(amp) amp.max=v('chargeMax');
    }
    const tileGrid=charts.cards[4].card.cards[1], prototype=tileGrid.cards[0];
    tileGrid.layout['grid-template-columns']=`repeat(${v('applianceColumns')},minmax(0,1fr))`;
    tileGrid.cards=p.appliances.map(a=>{
      const tile=clone(prototype);tile.name=a.name;tile.entity=p.entities[a.entity]||a.entity;tile.icon=a.icon;
      const ap=p.applianceColors||palette;
      const color=`const n=Number(entity?.state);if(!entity||['unknown','unavailable','none',''].includes(entity.state)||!Number.isFinite(n))return '#91a1ad';${a.thresholds.map(([n,c])=>`if(n>${Number(n)})return '${ap[c]}';`).join('')}return '${ap.green}';`;
      tile.styles.icon.find(x=>x.color).color='[[[ '+color+' ]]]';
      tile.styles.card.find(x=>x.height).height=v('applianceHeight')+'px';
      tile.styles.name.find(x=>x['font-size'])['font-size']=v('applianceLabel')+'px';
      tile.styles.state.find(x=>x['font-size'])['font-size']=v('applianceValue')+'px';return tile;
    });
    charts.cards[4].card.cards[0].name='Consumi per utenza · '+p.appliances.length+' monitorate';
    for (const a of p.actions) {
      const button=hero.custom_fields[a.key]?.card;if(!button) continue;const oldEntity=button.entity;
      button.entity=p.entities[a.entity]||a.entity;button.name=a.name;button.icon=a.icon;button.tooltip=a.tooltip;
      for(const state of button.state||[])if(typeof state.value==='string')state.value=a.condition?'[[[[ '+bind(a.condition).replaceAll(oldEntity,button.entity)+' ]]]]':state.value.replaceAll(oldEntity,button.entity);
      if(Array.isArray(button.triggers_update))button.triggers_update=button.triggers_update.map(id=>id===oldEntity?button.entity:id);
      button.tap_action={action:'call-service',service:bind(a.service),haptic:'medium',confirmation:{text:a.confirm}};
      if (a.service.startsWith('switch.')) button.tap_action.target={entity_id:button.entity};
      let top=`calc(${Number(a.y)}% + ${Number(a.y_offset||0)}px)`;
      if(a.minimumTop!=null) top=`max(${top},calc(48% + ${Number(a.minimumTop)}px))`;
      button.extra_styles=button.extra_styles.replace(/:host\s*\{[^}]*\}/,`:host{position:absolute!important;left:${Number(a.x)}%;top:${top};width:46px;height:46px;transform:translate(-50%,-50%);pointer-events:auto!important}`);
      button.extra_styles+=`@media(max-width:650px){:host{top:calc(${Number(a.y)}% + ${Number(a.y_offset||0)}px)!important}}`;
      hero.styles.custom_fields[a.key].find(x=>x.top).top=header+'px';
      for(const s of button.styles.icon)if(s.color)s.color=a.color;
    }
    if(!appliancesOpen){
      charts.cards.splice(4,1);
      charts.layout['grid-template-rows']=`auto minmax(${v('graphHeight')}px,1fr) auto minmax(${v('graphHeight')}px,1fr)`;
    }
    return dashboard;
  }

  const css=`
    :host{display:block;font-family:var(--primary-font-family,Roboto,Arial,sans-serif);color:#edf2f3}*{box-sizing:border-box}[hidden]{display:none!important}
    dialog{width:min(1140px,96vw);max-height:92dvh;padding:0;border:1px solid #ffffff30;border-radius:22px;background:#131d25;color:#edf2f3;box-shadow:0 28px 100px #000a;font:inherit}
    .fallback-scrim{position:fixed;inset:0;background:#03090ec9;backdrop-filter:blur(8px);z-index:2147483646}dialog[data-fallback]{display:block;position:fixed;left:50%;top:50%;transform:translate(-50%,-50%);margin:0;z-index:2147483647}
    dialog::backdrop{background:#03090ec9;backdrop-filter:blur(8px)}.head{position:sticky;top:0;z-index:3;background:#131d2afa;display:flex;align-items:center;gap:12px;padding:18px 22px;border-bottom:1px solid #ffffff15}.head h2{font-size:19px;margin:0;font-weight:500}.head small{display:block;font-size:11px;color:#9dafbc;margin-top:5px}.close{margin-left:auto}
    button,input,select,textarea{font:inherit}button{border:1px solid #ffffff25;border-radius:9px;padding:8px 12px;background:#ffffff08;color:#dce9ed;cursor:pointer}button:hover{background:#ffffff12}button:focus-visible,input:focus-visible,select:focus-visible,textarea:focus-visible{outline:2px solid #7dd9ba;outline-offset:2px}button.primary{background:#7dd9ba;color:#10251e;border:0;font-weight:600}.close{font-size:20px;padding:4px 12px}.tabs{display:flex;gap:6px;flex-wrap:wrap;padding:12px 20px;border-bottom:1px solid #ffffff12}.tabs button{font-size:12px;padding:7px 9px}.tabs button.active{background:#7dd9ba20;border-color:#7dd9ba66;color:#7dd9ba}.body{padding:18px 22px;max-height:65dvh;overflow:auto}.section[hidden]{display:none}.fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.field{display:flex;flex-direction:column;gap:6px;min-width:0}.field>span,.field>label{font-size:12px;color:#bccbd5;overflow-wrap:anywhere}.field small,.note{overflow-wrap:anywhere;font-size:11px;line-height:1.5;color:#94a8b7}.note{margin:0 0 15px}.field input,.field select,.field textarea{width:100%;min-height:36px;border:1px solid #ffffff25;border-radius:8px;background:#0b141b;color:#edf2f3;padding:7px 10px}.field input:disabled{opacity:.6}.field input[type=color]{padding:2px;height:36px}.source{display:flex;gap:6px}.source select{font-size:11px}.source button{font-size:11px;padding:5px 9px}.foot{position:sticky;bottom:0;background:#131d2afa;border-top:1px solid #ffffff18;padding:13px 22px;display:flex;align-items:center;gap:8px;flex-wrap:wrap}.status{font-size:12px;color:#aac1ce;margin-right:auto}.error{color:#ff9ca6}.hidden-file{display:none}.block{padding:12px;border:1px solid #ffffff14;border-radius:12px;margin-bottom:12px}.block summary{font-size:14px;color:#d9e4e9;cursor:pointer}.block .fields{margin-top:12px}.code{width:100%;min-height:420px;background:#081219;color:#d5e7ef;border:1px solid #ffffff25;border-radius:10px;font:12px/1.5 Consolas,monospace;padding:12px}.live{display:flex;align-items:center;gap:10px;margin-bottom:12px}.live label{font-size:12px;color:#afc2cf;min-width:165px}.live select,.live input{background:#0b141b;color:#edf2f3;border:1px solid #ffffff30;padding:8px;border-radius:8px}.load-error{padding:20px;background:#1b2029;border:1px solid #df8999;border-radius:15px;white-space:pre-wrap}.search{margin:0 0 12px;width:100%;background:#0b141b;color:#edf2f3;border:1px solid #ffffff25;border-radius:8px;padding:9px}@media(max-width:650px){.fields{grid-template-columns:1fr}.head,.body{padding:15px}.live{flex-wrap:wrap}.tabs{padding:10px}.foot{padding:12px}.body{max-height:58dvh}}`;

  class EnergyObsidianCard extends HTMLElement {
    constructor(){super();this.attachShadow({mode:'open'});this._owner='obsidian-'+(++sequence);owners.set(this._owner,this);this.shadowRoot.innerHTML='<style>'+css+'</style><div id="mount"></div>';this._bindUIEvents();}
    connectedCallback(){owners.set(this._owner,this);}
    disconnectedCallback(){owners.delete(this._owner);}
    setConfig(config){if(!config.config_url)throw new Error('config_url mancante');this._config=config;this._storage=config.storage_key||'energy-obsidian-profile:'+config.config_url;this._load();}
    set hass(hass){this._hass=hass;if(this._card)this._card.hass=hass;}
    get hass(){return this._hass;}
    getCardSize(){return 12;}
    _uiButton(event){return event.composedPath().find(node=>node?.getAttribute?.('data-obsidian-owner')===this._owner&&['settings','appliances','month-today','month-summary'].includes(node.getAttribute('data-obsidian-ui')));}
    _bindUIEvents(){
      const start=(event,point,id)=>{const button=this._uiButton(event);if(!button)return;this._uiPointer={action:button.dataset.obsidianUi,x:point.clientX,y:point.clientY,id,at:Date.now()};};
      const cancel=()=>{if(this._uiPointer)this._lastUIPointer={action:this._uiPointer.action,at:Date.now()};this._uiPointer=null;};
      const move=(point,id)=>{if(this._uiPointer?.id===id&&Math.hypot(point.clientX-this._uiPointer.x,point.clientY-this._uiPointer.y)>12)this._uiPointer.moved=true;};
      const end=(event,point,id)=>{
        const began=this._uiPointer;this._uiPointer=null;const button=this._uiButton(event);
        if(!began||!button||began.id!==id||began.action!==button.dataset.obsidianUi)return;
        if(began.moved||Math.hypot(point.clientX-began.x,point.clientY-began.y)>12||Date.now()-began.at>1000){this._lastUIPointer={action:began.action,at:Date.now()};return;}
        if(event.cancelable)event.preventDefault();event.stopImmediatePropagation();
        this._lastUIPointer={action:began.action,at:Date.now()};this._activateUI(began.action);
      };
      if(typeof window.PointerEvent==='function'){
        this.shadowRoot.addEventListener('pointerdown',event=>{if(event.isPrimary===false||(event.pointerType==='mouse'&&event.button!==0))return;start(event,event,event.pointerId);},{capture:true,passive:true});
        this.shadowRoot.addEventListener('pointerup',event=>{if(event.isPrimary===false)return;end(event,event,event.pointerId);},{capture:true});
        this.shadowRoot.addEventListener('pointermove',event=>move(event,event.pointerId),{capture:true,passive:true});
        this.shadowRoot.addEventListener('pointercancel',cancel,{capture:true,passive:true});
      }else{
        this.shadowRoot.addEventListener('touchstart',event=>{if(event.touches.length!==1){this._uiPointer=null;return;}start(event,event.touches[0],event.touches[0].identifier);},{capture:true,passive:true});
        this.shadowRoot.addEventListener('touchend',event=>{const touch=event.changedTouches[0];if(touch)end(event,touch,touch.identifier);},{capture:true});
        this.shadowRoot.addEventListener('touchmove',event=>{const touch=event.touches[0];if(touch)move(touch,touch.identifier);},{capture:true,passive:true});
        this.shadowRoot.addEventListener('touchcancel',cancel,{capture:true,passive:true});
      }
      this.shadowRoot.addEventListener('click',event=>{
        const button=this._uiButton(event);if(!button)return;
        if(event.cancelable)event.preventDefault();event.stopImmediatePropagation();
        const action=button.dataset.obsidianUi;
        // A synthetic mouse click may follow pointerup on touchscreens.
        if(event.detail!==0&&this._lastUIPointer?.action===action&&Date.now()-this._lastUIPointer.at<800)return;
        this._activateUI(action);
      },{capture:true});
    }
    _activateUI(action){if(action==='settings')this.openSettings();else if(action==='appliances')this.toggleAppliances();else if(action==='month-today')this.setMonthTab(false);else if(action==='month-summary')this.setMonthTab(true);}
    _showSettingsDialog(dialog){
      if(typeof dialog.showModal==='function'){try{dialog.showModal();return;}catch(error){/* Older embedded browsers may reject the top layer. */}}
      const scrim=document.createElement('div');scrim.className='fallback-scrim';dialog._backdrop=scrim;this.shadowRoot.insertBefore(scrim,dialog);
      dialog.dataset.fallback='true';dialog.setAttribute('open','');dialog.setAttribute('role','dialog');dialog.setAttribute('aria-modal','true');
      dialog.close=()=>{dialog.removeAttribute('open');dialog.hidden=true;scrim.remove();};scrim.onclick=()=>dialog.close();
      dialog.addEventListener('keydown',event=>{if(event.key==='Escape'){event.stopPropagation();dialog.close();}});
    }
    async _load(){
      try {
        const response=await fetch(this._config.config_url,{cache:'no-store'});if(!response.ok)throw new Error('HTTP '+response.status);
        this._baseline=validate(await response.json());let saved;
        try{const raw=localStorage.getItem(this._storage);if(raw)saved=validate(JSON.parse(raw));}catch(error){this._storageWarning='Il profilo locale non e valido: caricato il JSON esterno.';}
        this._profile=clone(saved||this._baseline);await this._renderCard();
      } catch(error){const box=this.shadowRoot.querySelector('#mount');box.innerHTML='<div class="load-error">Impossibile caricare il cockpit.\n'+escapeHTML(error.message)+'\nControlla il percorso del JSON esterno.</div>';}
    }
    async _renderCard(){const helpers=await window.loadCardHelpers();const card=helpers.createCardElement(materialize(this._profile,this._owner,{appliancesOpen:this._appliancesOpen,monthOpen:this._monthOpen}));if(this._hass)card.hass=this._hass;this.shadowRoot.querySelector('#mount').replaceChildren(card);this._card=card;}
    async toggleAppliances(){this._appliancesOpen=!this._appliancesOpen;await this._renderCard();}
    async setMonthTab(open){if(this._monthOpen===open)return;this._monthOpen=open;await this._renderCard();}
    _value(field){if(field.helper&&field.source!=='local'){const id=this._draft.entities[field.helper]||field.helper,raw=this._hass?.states[id]?.state;if(!unavailable(raw)&&finite(raw))return Number(raw);}return field.value;}
    _status(message,error=false){const s=this._dialog.querySelector('.status');s.textContent=message;s.classList.toggle('error',error);}
    _field(label,value,setter,options={}){
      const labelEl=document.createElement('div');labelEl.className='field';const caption=document.createElement('label');caption.textContent=label;labelEl.append(caption);
      const input=document.createElement('input');input.id=this._owner+'-field-'+(this._fieldSeq=(this._fieldSeq||0)+1);caption.htmlFor=input.id;input.type=options.type||'text';input.value=value??'';if(options.min!=null)input.min=options.min;if(options.max!=null)input.max=options.max;if(options.step!=null)input.step=options.step;
      input.addEventListener('change',()=>{setter(input.type==='number'?Number(input.value):input.value);this._dirty=true;this._status('Modifiche da applicare');});labelEl.append(input);return {el:labelEl,input};
    }
    _select(label,value,items,setter){const f=document.createElement('label');f.className='field';const t=document.createElement('span');t.textContent=label;f.append(t);const select=document.createElement('select');for(const [v,label] of items){const o=new Option(label,v);select.add(o);}select.value=value;select.onchange=()=>{setter(select.value);this._dirty=true;this._status('Modifiche da applicare');};f.append(select);return f;}
    _note(text){const n=document.createElement('p');n.className='note';n.textContent=text;return n;}
    _grid(parent){const grid=document.createElement('div');grid.className='fields';parent.append(grid);return grid;}
    _moreInfo(entity){this._pendingDraft=clone(this._draft);this._dialog?.close();this.dispatchEvent(new CustomEvent('hass-more-info',{detail:{entityId:entity},bubbles:true,composed:true}));}
    openSettings(draft=null,initialTab=null){
      this._dialog?._backdrop?.remove();this._dialog?.remove();this._draft=clone(draft||this._pendingDraft||this._profile);this._pendingDraft=null;this._dirty=false;const dialog=document.createElement('dialog');this._dialog=dialog;
      dialog.innerHTML='<div class="head"><div><h2>Impostazioni Obsidian</h2><small>Profilo locale del browser · JSON esterno esportabile</small></div><button class="close" aria-label="Chiudi impostazioni">×</button></div><nav class="tabs"></nav><div class="body"></div><div class="foot"><span class="status"></span><button data-action="import">Importa JSON</button><button data-action="export">Esporta JSON</button><button data-action="reset">Ripristina file esterno</button><button class="primary" data-action="apply">Salva e applica</button><input class="hidden-file" type="file" accept="application/json,.json"></div>';
      dialog.querySelector('.head small').textContent='Versione '+VERSION+' · Profilo locale del browser · JSON esterno esportabile';
      const close=()=>dialog.close();dialog.querySelector('.close').onclick=close;dialog.addEventListener('click',e=>{if(e.target===dialog)close();});
      const tabs=dialog.querySelector('.tabs'),body=dialog.querySelector('.body');
      const sections=new Map();const addTab=name=>{const button=document.createElement('button');button.textContent=name;tabs.append(button);const section=document.createElement('section');section.className='section';section.hidden=true;body.append(section);sections.set(name,{button,section});button.onclick=()=>{for(const [key,x] of sections){x.section.hidden=key!==name;x.button.classList.toggle('active',key===name);}};return section;};
      const groups=[...new Set(Object.values(this._draft.params).map(f=>f.group))];
      for(const group of groups){const section=addTab(group);const grid=this._grid(section);for(const [key,f] of Object.entries(this._draft.params).filter(([,f])=>f.group===group)){
        const field=this._field(f.label+(f.unit?' · '+f.unit:''),this._value(f),value=>{f.value=value;},{type:f.type==='number'?'number':'text',min:f.min,max:f.max,step:f.step});grid.append(field.el);
        if(f.helper){const controls=document.createElement('div');controls.className='source';const select=document.createElement('select');select.add(new Option('Helper HA (fallback profilo)','helper'));if(!f.haOnly)select.add(new Option('Valore del profilo locale','local'));select.value=f.source||'helper';field.input.disabled=select.value==='helper';select.onchange=()=>{f.source=select.value;field.input.disabled=f.source==='helper';field.input.value=this._value(f);this._dirty=true;this._status('Sorgente modificata');};const open=document.createElement('button');open.type='button';open.textContent='Apri helper';open.onclick=e=>{e.preventDefault();this._moreInfo(this._draft.entities[f.helper]||f.helper);};controls.append(select,open);field.el.append(controls);const small=document.createElement('small');small.textContent=f.helper;field.el.append(small);}
      }}
      const entities=addTab('Entita');entities.append(this._note('Associazioni condivise da foto, grafici, utenze e comandi. I nomi originali identificano i collegamenti.'));const search=document.createElement('input');search.className='search';search.placeholder='Filtra entita';entities.append(search);const eg=this._grid(entities);
      for(const [key,value] of Object.entries(this._draft.entities)){const field=this._field(key,value,value=>this._draft.entities[key]=value);field.el.dataset.search=key.toLowerCase();eg.append(field.el);}search.oninput=()=>eg.querySelectorAll('.field').forEach(el=>el.hidden=!el.dataset.search.includes(search.value.toLowerCase()));
      const photo=addTab('Foto e colori'),ig=this._grid(photo);for(const [key,value] of Object.entries(this._draft.images))ig.append(this._field('Immagine · '+key,value,x=>this._draft.images[key]=x).el);for(const [key,color] of Object.entries(this._draft.colors))ig.append(this._field(color.label,color.value,value=>color.value=value,{type:'color'}).el);
      for(const [key,value] of Object.entries(this._draft.applianceColors||palette))ig.append(this._field('Colore utenze · '+key,value,value=>{this._draft.applianceColors=this._draft.applianceColors||clone(palette);this._draft.applianceColors[key]=value;},{type:'color'}).el);
      for(const [key,f] of Object.entries(this._draft.photo)){const block=document.createElement('details');block.className='block';block.innerHTML='<summary>'+escapeHTML('Box '+key+' e collegamento')+'</summary>';const grid=this._grid(block);for(const [prop,value] of Object.entries(f))grid.append(this._field(prop,value,x=>f[prop]=x,{type:prop==='path'?'text':'number',step:.1}).el);photo.append(block);}
      const appliances=addTab('Soglie utenze');appliances.append(this._note('Confronti applicati in ordine rosso, arancione, giallo; verde sotto soglia. L’ordine viene conservato anche se due soglie si sovrappongono.'));
      for(const a of this._draft.appliances){const block=document.createElement('details');block.className='block';block.innerHTML='<summary>'+escapeHTML(a.name)+'</summary>';const grid=this._grid(block);for(const key of ['name','entity','icon'])grid.append(this._field(key,a[key],v=>a[key]=v).el);a.thresholds.forEach((t,i)=>grid.append(this._field('Oltre '+t[1]+' (W)',t[0],v=>a.thresholds[i][0]=v,{type:'number',min:0,step:1}).el));appliances.append(block);}
      const actions=addTab('Cancelli');actions.append(this._note('Coordinate in percentuale della foto. Gli offset verticali sono in pixel. Ogni comando mantiene la conferma prima dell’esecuzione.'));
      for(const a of this._draft.actions){const block=document.createElement('details');block.className='block';block.innerHTML='<summary>'+escapeHTML(a.name)+'</summary>';const grid=this._grid(block);for(const key of ['name','entity','service','icon','tooltip','confirm','x','y','y_offset','minimumTop'])if(a[key]!=null)grid.append(this._field(key,a[key],v=>a[key]=v,{type:typeof a[key]==='number'?'number':'text',step:.1}).el);actions.append(block);}
      const live=addTab('Inverter live');this._renderLive(live);
      const advanced=addTab('Avanzate');advanced.append(this._note('Profilo completo, suddiviso in sezioni JSON. Sono accessibili anche regole meteo, CSS, testi e struttura delle schede. Premi Usa questo JSON per acquisire le modifiche alla sezione, poi Salva e applica.'));
      const part=document.createElement('select');part.className='search';part.setAttribute('aria-label','Sezione JSON');
      const labels={params:'Parametri',entities:'Associazioni entita',images:'Immagini',weatherRules:'Regole meteo',photo:'Geometria foto',actions:'Comandi cancelli',appliances:'Utenze e soglie',colors:'Colori cockpit',applianceColors:'Colori utenze',dashboard:'Struttura completa delle schede'};
      for(const [key,label] of Object.entries(labels))part.add(new Option(label,key));advanced.append(part);
      const raw=document.createElement('textarea');raw.className='code';raw.spellcheck=false;raw.wrap='off';raw.setAttribute('aria-label','JSON della sezione');advanced.append(raw);
      const loadPart=()=>raw.value=JSON.stringify(this._draft[part.value],null,2);part.onchange=loadPart;loadPart();
      const pull=document.createElement('button');pull.textContent='Ricarica sezione';pull.onclick=loadPart;
      const commit=document.createElement('button');commit.textContent='Usa questo JSON';commit.onclick=()=>{try{const candidate=clone(this._draft);candidate[part.value]=JSON.parse(raw.value);validate(candidate);const selected=part.value;this.openSettings(candidate,'Avanzate');const next=this._dialog.querySelector('[aria-label="Sezione JSON"]');next.value=selected;next.dispatchEvent(new Event('change'));this._dirty=true;this._status('JSON valido. Premi Salva e applica.');}catch(e){this._status(e.message,true);}};advanced.append(pull,commit);
      dialog.querySelector('[data-action=apply]').onclick=async()=>{try{validate(this._draft);materialize(this._draft,this._owner);localStorage.setItem(this._storage,JSON.stringify(this._draft));this._profile=clone(this._draft);await this._renderCard();this._dirty=false;this._status('Profilo salvato e applicato su questo browser.');}catch(e){this._status(e.message,true);}};
      dialog.querySelector('[data-action=export]').onclick=()=>{try{validate(this._draft);this._download(this._draft);this._status('JSON esportato. Copialo sul server per condividerlo fra dispositivi.');}catch(e){this._status(e.message,true);}};
      dialog.querySelector('[data-action=import]').onclick=()=>dialog.querySelector('input[type=file]').click();
      dialog.querySelector('input[type=file]').onchange=async event=>{try{const file=event.target.files[0];if(!file)return;const candidate=validate(JSON.parse(await file.text()));this.openSettings(candidate,'Avanzate');this._dirty=true;this._status('Profilo importato: premi Salva e applica.');}catch(e){this._status(e.message,true);}};
      dialog.querySelector('[data-action=reset]').onclick=async()=>{try{this._profile=clone(this._baseline);localStorage.removeItem(this._storage);await this._renderCard();dialog.close();this.openSettings();this._status('Ripristinato il JSON esterno.');}catch(e){this._status(e.message,true);}};
      this.shadowRoot.append(dialog);sections.get(initialTab||groups[0]).button.click();this._showSettingsDialog(dialog);this._status(this._storageWarning||'Modifica un valore, poi salva e applica.');
    }
    _download(p){const url=URL.createObjectURL(new Blob([JSON.stringify(p,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='energy-obsidian-config.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
    _renderLive(section){
      section.append(this._note('Questi controlli scrivono le entita reali. Le modifiche al profilo grafico non inviano comandi all’inverter.'));
      const defs=[['Modalita EMS','select.solarman_modalita_ems','select','select_option','option'],['Corrente massima (A)','number.solarman_corrente_max_carica','number','set_value','value']];
      for(const [label,original,domain,service,arg] of defs){const id=this._draft.entities[original]||original,state=this._hass?.states[id],row=document.createElement('div');row.className='live';const title=document.createElement('label');title.textContent=label;row.append(title);let input;
        if(domain==='select'){input=document.createElement('select');for(const value of state?.attributes?.options||['SelfUse','ChgFst','BatFst'])input.add(new Option(value,value));input.value=state?.state||'';}else{input=document.createElement('input');input.type='number';input.min=state?.attributes?.min??0;input.max=state?.attributes?.max??this._draft.params.chargeMax.value;input.step=state?.attributes?.step??1;input.value=state?.state??'';}
        const apply=document.createElement('button');apply.textContent='Applica';apply.disabled=!state;apply.onclick=async()=>{try{if(!this._hass)throw new Error('Home Assistant non collegato.');await this._hass.callService(domain,service,{entity_id:id,[arg]:domain==='number'?Number(input.value):input.value});this._status('Comando applicato: '+id);}catch(e){this._status(e.message,true);}};row.append(input,apply);section.append(row);
      }
      const id=this._draft.entities['automation.solarman_smart_energy_manager']||'automation.solarman_smart_energy_manager',state=this._hass?.states[id],row=document.createElement('div');row.className='live';row.textContent='Automazione SEM: '+(state?.state||'non disponibile');for(const [label,service] of [['Attiva','turn_on'],['Disattiva','turn_off']]){const b=document.createElement('button');b.textContent=label;b.disabled=!state;b.onclick=async()=>{try{await this._hass.callService('automation',service,{entity_id:id});this._status('Automazione aggiornata');}catch(e){this._status(e.message,true);}};row.append(b);}section.append(row);
    }
  }
  window.EnergyObsidian={version:VERSION,open:id=>owners.get(id)?.openSettings(),toggleAppliances:id=>owners.get(id)?.toggleAppliances(),materialize,validate};
  if(!customElements.get('energy-obsidian-card'))customElements.define('energy-obsidian-card',EnergyObsidianCard);
  window.customCards=window.customCards||[];window.customCards.push({type:'energy-obsidian-card',name:'Obsidian Energy',description:'Cockpit con configurazione esterna e impostazioni guidate'});
})();
