# =====================================================================
#  MiniDisplay Bridge
#  Releve le quota Claude (session 5 h + semaine) et l'etat de
#  status.claude.com, puis l'envoie a la carte ESP32-C6-LCD-1.47 par l'USB.
#
#  Meme source que Claude Bot Widget :
#    GET https://api.anthropic.com/api/oauth/usage (non documente)
#    jeton depose par Claude Code dans %USERPROFILE%\.claude\.credentials.json
#
#  Protocole (une ligne, ASCII) :
#    Q;<now>;<five%>;<fiveReset>;<week%>;<weekReset>;<level>;<updated>;<erreur>
#  Heures en secondes "heure locale" depuis 1970 (la carte n'a pas de fuseau),
#  -1 / 0 = inconnu. level : -1 injoignable, 0 ok, 1 degrade, 2 panne.
#
#  Option ElevenLabs : si ELEVENLABS_API_KEY est defini (variable
#  d'environnement ou bridge/secrets.env, jamais committe), une seconde ligne :
#    E;<utilises>;<limite>;<reset>;<updated>;<erreur>
#  La cle doit avoir la permission "Utilisateur : Acces" (user_read).
#
#  Ressources du PC, toutes les 3 s (non journalise) :
#    P;<cpu%>;<ramUtiliseeMo>;<ramTotaleMo>;<gpu%>;<gpuTemp>
#  GPU lu par nvidia-smi (cartes NVIDIA), -1 si absent.
#
#  Option PALais SIAM : si PALAIS_SIAM_URL, _KEY, _EMAIL et _PASSWORD sont
#  definis (bridge/secrets.env), le pont se connecte au Supabase du site avec
#  ce compte et guette les nouvelles commandes toutes les 20 s :
#    N;<nombre>;<noms>;<plat>
#  et envoie chaque minute le resume de la semaine en cours (vue PALais SIAM) :
#    S;<date>;<limite>;<ouvert>;<commandes>;<payees>;<devinettes>;<bonnes>;<total>;<derniers>
#  <bonnes> vaut -1 tant que la solution n'est pas revelee.
#
#  Bourse (8e vue) : MARKET_SYMBOL dans bridge/settings.env (ticker Yahoo,
#  ou BTC / ETH / SOL), cours releve chaque minute, sans cle :
#    M;<symbole>;<nom>;<cours>;<variation%>;<devise>;<ouvert>;<releve>;<min>;<max>;<points>
#  <points> : la seance (ou les dernieres 24 h en crypto), 0 a 99 entre min et max.
#
#  Parametres :
#    -Port COM3   force le port (sinon detection par VID Espressif 303A)
#    -Once        un seul envoi puis sortie (test)
# =====================================================================
param(
    [string]$Port,
    [switch]$Once
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System

$QuotaUrl  = 'https://api.anthropic.com/api/oauth/usage'
$StatusUrl = 'https://status.claude.com/api/v2/summary.json'

# Garde-fous contre le rate-limit, identiques au widget : un appel toutes les
# 5 minutes au plus, attente doublee a chaque 429 (10, 20, 40, puis 60 min).
$QuotaMinMinutes  = 5
$QuotaMaxMinutes  = 60
$StatusMinutes    = 5
$SendEverySeconds = 30

$ElevenUrl     = 'https://api.elevenlabs.io/v1/user/subscription'
$ElevenMinutes = 10
$PcSeconds     = 3
$OrderSeconds  = 20
$MarketSeconds = 60
$MarketPoints  = 60
$SummarySeconds = 60
$NvidiaSmi     = (Get-Command nvidia-smi -ErrorAction SilentlyContinue).Source

$LogPath = Join-Path $PSScriptRoot 'bridge.log'

function Write-Log {
    param([string]$Msg)
    $line = '{0:yyyy-MM-dd HH:mm:ss}  {1}' -f (Get-Date), $Msg
    Write-Host $line
    try {
        if ((Test-Path $LogPath) -and (Get-Item $LogPath).Length -gt 512KB) { Remove-Item $LogPath -Force }
        Add-Content -Path $LogPath -Value $line -Encoding UTF8
    } catch { }
}

# ---------------- Reseau ----------------
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.ServicePointManager]::SecurityProtocol
} catch { }

function Invoke-Get {
    param([string]$Url, [hashtable]$Headers, [string]$UA, [string]$Body)
    $r = @{ Code = 0; Raw = $null; Err = $null }
    try {
        $req = [System.Net.HttpWebRequest]::Create($Url)
        $req.Method           = if ($Body) { 'POST' } else { 'GET' }
        $req.Timeout          = 20000
        $req.ReadWriteTimeout = 20000
        $req.UserAgent        = $UA
        $req.Accept           = 'application/json'
        $req.CachePolicy      = New-Object System.Net.Cache.RequestCachePolicy([System.Net.Cache.RequestCacheLevel]::NoCacheNoStore)
        foreach ($k in $Headers.Keys) { $req.Headers.Add($k, $Headers[$k]) }
        try {
            $px = [System.Net.WebRequest]::DefaultWebProxy
            if ($px) { $px.Credentials = [System.Net.CredentialCache]::DefaultNetworkCredentials; $req.Proxy = $px }
        } catch { }
        if ($Body) {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
            $req.ContentType = 'application/json'
            $st = $req.GetRequestStream(); $st.Write($bytes, 0, $bytes.Length); $st.Close()
        }
        $resp = $req.GetResponse()
        $sr   = New-Object System.IO.StreamReader($resp.GetResponseStream(), [System.Text.Encoding]::UTF8)
        $r.Raw = $sr.ReadToEnd()
        $sr.Close(); $resp.Close()
        $r.Code = 200
    } catch {
        $r.Err = $_.Exception.Message
        try {
            $ex = $_.Exception
            if ($ex.InnerException) { $ex = $ex.InnerException }
            if ($ex.Response) { $r.Code = [int]$ex.Response.StatusCode }
        } catch { }
    }
    return $r
}

function Get-OauthToken {
    if ($env:CLAUDE_CODE_OAUTH_TOKEN) { return ([string]$env:CLAUDE_CODE_OAUTH_TOKEN).Trim() }
    $f = Join-Path $env:USERPROFILE '.claude\.credentials.json'
    if (Test-Path $f) {
        try {
            $o = (Get-Content -Path $f -Raw -Encoding UTF8) | ConvertFrom-Json
            if ($o.claudeAiOauth -and $o.claudeAiOauth.accessToken) { return [string]$o.claudeAiOauth.accessToken }
        } catch { }
    }
    return $null
}

function Get-Secret {
    # Variable d'environnement, sinon ligne NOM=valeur de bridge/settings.env ou secrets.env
    param([string]$Name)
    $v = [Environment]::GetEnvironmentVariable($Name)
    if ($v) { return $v.Trim() }
    foreach ($file in 'settings.env', 'secrets.env') {
        $f = Join-Path $PSScriptRoot $file
        if (-not (Test-Path $f)) { continue }
        foreach ($l in Get-Content -Path $f -Encoding UTF8) {
            if ($l -match "^\s*$Name\s*=\s*(.+?)\s*$") { return $Matches[1].Trim('"', "'") }
        }
    }
    return $null
}

function Get-ElevenKey { return Get-Secret 'ELEVENLABS_API_KEY' }

# ---------------- Etat ----------------
$Eleven = @{ Used = -1; Limit = -1; Reset = $null; Updated = $null; Error = 'premier releve en cours' }
$ElevenKey  = Get-ElevenKey
$NextEleven = Get-Date
$NextPc     = Get-Date
$Siam = @{
    Url = Get-Secret 'PALAIS_SIAM_URL'; Key = Get-Secret 'PALAIS_SIAM_KEY'
    Email = Get-Secret 'PALAIS_SIAM_EMAIL'; Password = Get-Secret 'PALAIS_SIAM_PASSWORD'
    Token = $null; Expires = Get-Date; UserId = $null; LastId = -1
}
$SiamOn    = [bool]($Siam.Url -and $Siam.Key -and $Siam.Email -and $Siam.Password)
$NextOrder = Get-Date
$NextSummary  = Get-Date
$MarketSymbol = Get-Secret 'MARKET_SYMBOL'
$NextMarket   = Get-Date
$MarketFrame  = $null
$SummaryFrame = $null
$Quota = @{ Five = -1; FiveReset = $null; Week = -1; WeekReset = $null; Updated = $null; Error = 'premier releve en cours'; Code = 0 }
$Level = -1
$QuotaBackoff = 0
$NextQuota  = Get-Date
$NextStatus = Get-Date

function ConvertTo-LocalTime {
    param([string]$Iso)
    if (-not $Iso) { return $null }
    try {
        $styles = [System.Globalization.DateTimeStyles]::AdjustToUniversal -bor [System.Globalization.DateTimeStyles]::AssumeUniversal
        $d = ([datetime]::Parse($Iso, [System.Globalization.CultureInfo]::InvariantCulture, $styles)).ToLocalTime()
        # L'API renvoie souvent 17:59:59.9 pour 18:00 : arrondi a la minute.
        return $d.AddSeconds(30).AddTicks(-($d.AddSeconds(30).Ticks % [TimeSpan]::TicksPerMinute))
    } catch { return $null }
}

function Update-Quota {
    $now = Get-Date
    $tok = Get-OauthToken
    if (-not $tok) {
        $script:Quota.Error = 'aucun jeton Claude Code sur ce poste'
        $script:NextQuota = $now.AddMinutes(15)
        return
    }
    $script:NextQuota = $now.AddMinutes($QuotaMinMinutes)
    # Le User-Agent compte : sans lui l'endpoint rate-limite beaucoup plus vite.
    $r = Invoke-Get -Url $QuotaUrl -UA 'claude-code/1.0.0' -Headers @{
        'Authorization'  = 'Bearer ' + $tok
        'anthropic-beta' = 'oauth-2025-04-20'
    }
    $script:Quota.Code = [int]$r.Code

    if ($r.Code -eq 429) {
        if ($script:QuotaBackoff -lt $QuotaMinMinutes) { $script:QuotaBackoff = $QuotaMinMinutes }
        $script:QuotaBackoff = [math]::Min($QuotaMaxMinutes, $script:QuotaBackoff * 2)
        $script:NextQuota = $now.AddMinutes($script:QuotaBackoff)
        $script:Quota.Error = "trop d'appels, nouvel essai dans $($script:QuotaBackoff) min"
        return
    }
    if ($r.Code -eq 401 -or $r.Code -eq 403) {
        $script:Quota.Error = 'jeton expire, lancez Claude Code'
        $script:NextQuota = $now.AddMinutes(15)
        return
    }
    if ($r.Code -ne 200 -or -not $r.Raw) { $script:Quota.Error = "quota indisponible ($($r.Code))"; return }
    try { $o = $r.Raw | ConvertFrom-Json } catch { $script:Quota.Error = 'reponse illisible'; return }

    $script:QuotaBackoff = 0
    $q = $script:Quota
    $q.Error = $null; $q.Five = -1; $q.Week = -1; $q.FiveReset = $null; $q.WeekReset = $null
    if ($o.five_hour) {
        if ($null -ne $o.five_hour.utilization) { $q.Five = [int][math]::Round([double]$o.five_hour.utilization) }
        $q.FiveReset = ConvertTo-LocalTime -Iso ([string]$o.five_hour.resets_at)
    }
    if ($o.seven_day) {
        if ($null -ne $o.seven_day.utilization) { $q.Week = [int][math]::Round([double]$o.seven_day.utilization) }
        $q.WeekReset = ConvertTo-LocalTime -Iso ([string]$o.seven_day.resets_at)
    }
    $q.Updated = $now
    if ($q.Five -lt 0 -and $q.Week -lt 0) { $q.Error = 'reponse sans quota exploitable' }
}

function Update-Eleven {
    $now = Get-Date
    $script:NextEleven = $now.AddMinutes($ElevenMinutes)
    $r = Invoke-Get -Url $ElevenUrl -UA 'MiniDisplayBridge/1.0' -Headers @{ 'xi-api-key' = $ElevenKey }
    $e = $script:Eleven
    if ($r.Code -eq 401 -or $r.Code -eq 403) { $e.Error = 'cle refusee (permission Utilisateur ?)'; $e.Used = -1; return }
    if ($r.Code -ne 200 -or -not $r.Raw) { $e.Error = "ElevenLabs indisponible ($($r.Code))"; return }
    try { $o = $r.Raw | ConvertFrom-Json } catch { $e.Error = 'reponse illisible'; return }
    $e.Used  = [int64]$o.character_count
    $e.Limit = [int64]$o.character_limit
    $e.Reset = if ($o.next_character_count_reset_unix) { [DateTimeOffset]::FromUnixTimeSeconds([int64]$o.next_character_count_reset_unix).LocalDateTime } else { $null }
    $e.Updated = $now
    $e.Error = $null
}

# ---------------- PALais SIAM ----------------
# Connexion avec le compte de l'organisateur (meme email / mot de passe que sur
# le site) : les regles du site laissent tout compte connecte lire les commandes.
# Windows PowerShell 5.1 rend une liste JSON comme un seul objet : on la deroule
function ConvertFrom-JsonRows {
    param([string]$Raw)
    return @(($Raw | ConvertFrom-Json) | ForEach-Object { $_ })
}

function Connect-Siam {
    $s = $script:Siam
    $body = @{ email = $s.Email; password = $s.Password } | ConvertTo-Json -Compress
    $r = Invoke-Get -Url "$($s.Url)/auth/v1/token?grant_type=password" -UA 'MiniDisplayBridge/1.0' -Body $body -Headers @{ 'apikey' = $s.Key }
    if ($r.Code -ne 200) {
        Write-Log "PALais SIAM : connexion refusee ($($r.Code)), verifiez email et mot de passe"
        return $false
    }
    $o = $r.Raw | ConvertFrom-Json
    $s.Token = [string]$o.access_token
    $s.UserId = [string]$o.user.id
    $s.Expires = (Get-Date).AddSeconds([int]$o.expires_in - 120)
    return $true
}

function Update-Orders {
    $script:NextOrder = (Get-Date).AddSeconds($OrderSeconds)
    $s = $script:Siam
    if (-not $s.Token -or (Get-Date) -ge $s.Expires) {
        if (-not (Connect-Siam)) { $script:NextOrder = (Get-Date).AddMinutes(10); return }
    }
    $h = @{ 'apikey' = $s.Key; 'Authorization' = 'Bearer ' + $s.Token }
    $base = "$($s.Url)/rest/v1/orders"
    if ($s.LastId -lt 0) {
        # Premier passage : on part de la derniere commande, sans annoncer l'existant
        $r = Invoke-Get -Url "$base`?select=id&order=id.desc&limit=1" -Headers $h -UA 'MiniDisplayBridge/1.0'
        if ($r.Code -ne 200) { $s.Token = $null; return }
        $rows = @(ConvertFrom-JsonRows $r.Raw)
        $s.LastId = if ($rows.Count) { [int64]$rows[0].id } else { 0 }
        Write-Log "PALais SIAM : connecte, derniere commande n. $($s.LastId)"
        return
    }
    # Toutes les commandes, y compris les siennes et celles saisies a la main
    $q = "$base`?select=id,guest_name,for_name,profiles(display_name),dishes(name)&id=gt.$($s.LastId)&order=id.asc"
    $r = Invoke-Get -Url $q -Headers $h -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -eq 401) { $s.Token = $null; return }
    if ($r.Code -ne 200) { return }
    $rows = @(ConvertFrom-JsonRows $r.Raw)
    if (-not $rows.Count) { return }
    $s.LastId = [int64]($rows | Select-Object -Last 1).id
    $names = foreach ($o in $rows) {
        $who = if ($o.profiles) { [string]$o.profiles.display_name } else { "Quelqu'un" }
        if ($o.guest_name) { [string]$o.guest_name }
        elseif ($o.for_name) { "$($o.for_name) (via $who)" } else { $who }
    }
    $dish = if ($rows.Count -eq 1 -and $rows[0].dishes) { [string]$rows[0].dishes.name } else { '' }
    Send-Order -Count $rows.Count -Names ($names -join ', ') -Dish $dish
    $script:NextSummary = Get-Date
}

# Resume de la semaine en cours : la plus recente, ouverte ou non
function Update-Summary {
    $script:NextSummary = (Get-Date).AddSeconds($SummarySeconds)
    $s = $script:Siam
    if (-not $s.Token -or (Get-Date) -ge $s.Expires) {
        if (-not (Connect-Siam)) { return }
    }
    $h = @{ 'apikey' = $s.Key; 'Authorization' = 'Bearer ' + $s.Token }
    $api = "$($s.Url)/rest/v1"
    $r = Invoke-Get -Url "$api/weeks?select=id,order_date,deadline,status,answer_revealed&order=order_date.desc&limit=1" -Headers $h -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -eq 401) { $s.Token = $null; return }
    if ($r.Code -ne 200) { return }
    $weeks = @(ConvertFrom-JsonRows $r.Raw)
    if (-not $weeks.Count) { return }
    $w = $weeks[0]

    $r = Invoke-Get -Url "$api/orders?select=id,paid,guest_name,for_name,profiles(display_name)&week_id=eq.$($w.id)&order=id.desc" -Headers $h -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -ne 200) { return }
    $orders = @(ConvertFrom-JsonRows $r.Raw)
    # En organisateur, toutes les reponses sont lisibles
    $r = Invoke-Get -Url "$api/guesses?select=is_correct&week_id=eq.$($w.id)" -Headers $h -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -ne 200) { return }
    $guesses = @(ConvertFrom-JsonRows $r.Raw)
    $r = Invoke-Get -Url "$api/orders?select=id" -Headers $h -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -ne 200) { return }
    $total = @(ConvertFrom-JsonRows $r.Raw).Count

    $paid = @($orders | Where-Object { $_.paid }).Count
    $correct = if ($w.answer_revealed) { @($guesses | Where-Object { $_.is_correct }).Count } else { -1 }
    $recent = foreach ($o in ($orders | Select-Object -First 3)) {
        if ($o.guest_name) { [string]$o.guest_name }
        elseif ($o.for_name) { [string]$o.for_name }
        elseif ($o.profiles) { [string]$o.profiles.display_name }
    }
    $date = [datetime]::ParseExact([string]$w.order_date, 'yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture)
    $deadline = ConvertTo-LocalTime -Iso ([string]$w.deadline)
    $script:SummaryFrame = 'S;{0};{1};{2};{3};{4};{5};{6};{7};{8}' -f `
        (ConvertTo-LocalEpoch $date), (ConvertTo-LocalEpoch $deadline), $(if ($w.status -eq 'open') { 1 } else { 0 }),
        $orders.Count, $paid, $guesses.Count, $correct, $total, (ConvertTo-Ascii ($recent -join ', '))
    Send-Summary
}

function Send-Summary {
    if (-not $script:SummaryFrame) { return }
    if (-not $script:Serial -and -not (Open-Board)) { return }
    try { $script:Serial.WriteLine($script:SummaryFrame) } catch { Close-Board }
}

function Send-Order {
    param([int]$Count, [string]$Names, [string]$Dish)
    if (-not $script:Serial -and -not (Open-Board)) { return }
    $f = 'N;{0};{1};{2}' -f $Count, (ConvertTo-Ascii $Names), (ConvertTo-Ascii $Dish)
    try { $script:Serial.WriteLine($f); Write-Log "-> $f" } catch { Close-Board }
}

# ---------------- Bourse ----------------
# API de graphiques de Yahoo Finance, publique et sans cle. Les cryptos
# s'ecrivent BTC-USD ; BTC, ETH et SOL seuls sont completes.
function Update-Market {
    $script:NextMarket = (Get-Date).AddSeconds($MarketSeconds)
    $sym = $MarketSymbol.ToUpper()
    if ($sym -in 'BTC', 'ETH', 'SOL') { $sym = "$sym-USD" }
    $crypto = $sym -like '*-USD'
    $url = "https://query1.finance.yahoo.com/v8/finance/chart/$sym`?range=1d&interval=5m"
    $r = Invoke-Get -Url $url -Headers @{} -UA 'Mozilla/5.0'
    if ($r.Code -ne 200 -or -not $r.Raw) { Write-Log "Bourse : $sym indisponible ($($r.Code))"; return }
    try { $res = ($r.Raw | ConvertFrom-Json).chart.result[0] } catch { return }
    if (-not $res) { return }
    $m = $res.meta
    $closes = @($res.indicators.quote[0].close | Where-Object { $null -ne $_ } | ForEach-Object { [double]$_ })
    $price = [double]$m.regularMarketPrice
    # Reference : cloture de la veille en bourse ; en crypto (24 h/24), debut de la journee
    $ref = if ($crypto -and $closes.Count) { $closes[0] } else { [double]$m.chartPreviousClose }
    $pct = if ($ref) { ($price - $ref) / $ref * 100 } else { 0 }
    $nowUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $p = $m.currentTradingPeriod.regular
    $open = if ($crypto) { 1 } elseif ($nowUnix -ge [int64]$p.start -and $nowUnix -lt [int64]$p.end) { 1 } else { 0 }

    # Courbe : au plus $MarketPoints points, ramenes entre 0 et 99
    $pts = ''; $min = 0; $max = 0
    if ($closes.Count -ge 2) {
        $step = [math]::Max(1, [math]::Ceiling($closes.Count / $MarketPoints))
        $sample = for ($i = 0; $i -lt $closes.Count; $i += $step) { $closes[$i] }
        $sample = @($sample) + $price
        $min = ($sample | Measure-Object -Minimum).Minimum
        $max = ($sample | Measure-Object -Maximum).Maximum
        $span = if ($max -gt $min) { $max - $min } else { 1 }
        $pts = ($sample | ForEach-Object { [int][math]::Round(($_ - $min) / $span * 99) }) -join ','
    }
    $inv = [Globalization.CultureInfo]::InvariantCulture
    $label = $MarketSymbol.ToUpper() -replace '-USD$', ''
    $name = ([string]$m.shortName) -replace ' USD$', ''  # « Bitcoin USD » -> « Bitcoin »
    $script:MarketFrame = 'M;{0};{1};{2};{3};{4};{5};{6};{7};{8};{9}' -f $label, (ConvertTo-Ascii $name),
        (Format-Price $price), $pct.ToString('0.00', $inv), [string]$m.currency, $open,
        (ConvertTo-LocalEpoch (Get-Date)), (Format-Price $min), (Format-Price $max), $pts
    Send-Market
}

# 81501.52 -> « 81 502 » ; 13.21 -> « 13.21 »
function Format-Price {
    param([double]$V)
    $inv = [Globalization.CultureInfo]::InvariantCulture
    if ($V -ge 1000) { return $V.ToString('#,0', $inv).Replace(',', ' ') }
    return $V.ToString('0.00', $inv)
}

function Send-Market {
    if (-not $script:MarketFrame) { return }
    if (-not $script:Serial -and -not (Open-Board)) { return }
    try { $script:Serial.WriteLine($script:MarketFrame) } catch { Close-Board }
}

function Get-PcFrame {
    # Processeur et memoire par WMI (noms de classes non traduits, contrairement aux compteurs)
    $cpu = -1; $used = -1; $total = -1; $gpu = -1; $temp = -1
    try { $cpu = [int](Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -Filter "Name='_Total'").PercentProcessorTime } catch { }
    try {
        $o = Get-CimInstance Win32_OperatingSystem
        $total = [int]($o.TotalVisibleMemorySize / 1024)
        $used = [int](($o.TotalVisibleMemorySize - $o.FreePhysicalMemory) / 1024)
    } catch { }
    if ($NvidiaSmi) {
        try {
            $g = (& $NvidiaSmi --query-gpu=utilization.gpu,temperature.gpu --format=csv,noheader,nounits | Select-Object -First 1) -split ','
            $gpu = [int]$g[0].Trim(); $temp = [int]$g[1].Trim()
        } catch { }
    }
    return 'P;{0};{1};{2};{3};{4}' -f $cpu, $used, $total, $gpu, $temp
}

function Send-Pc {
    if (-not $script:Serial -and -not (Open-Board)) { return }
    try { $script:Serial.WriteLine((Get-PcFrame)) } catch { Close-Board }
}

function Update-Status {
    $script:NextStatus = (Get-Date).AddMinutes($StatusMinutes)
    $r = Invoke-Get -Url $StatusUrl -Headers @{} -UA 'MiniDisplayBridge/1.0'
    if ($r.Code -ne 200 -or -not $r.Raw) { $script:Level = -1; return }
    try {
        $ind = [string](($r.Raw | ConvertFrom-Json).status.indicator)
        $script:Level = switch ($ind) { 'none' { 0 } 'minor' { 1 } 'maintenance' { 1 } 'major' { 2 } 'critical' { 2 } default { -1 } }
    } catch { $script:Level = -1 }
}

# ---------------- Trame ----------------
$Epoch = New-Object DateTime 1970, 1, 1, 0, 0, 0, ([DateTimeKind]::Unspecified)

function ConvertTo-LocalEpoch {
    # Secondes depuis 1970 de l'heure locale "a plat" : la carte l'affiche telle quelle.
    param($Date)
    if (-not $Date) { return 0 }
    $d = [datetime]::SpecifyKind([datetime]$Date, [DateTimeKind]::Unspecified)
    return [int64][math]::Floor(($d - $Epoch).TotalSeconds)
}

function ConvertTo-Ascii {
    # La police de la carte n'a que l'ASCII : on retire accents et separateurs.
    param([string]$Text)
    if (-not $Text) { return '' }
    $n = $Text.Normalize([Text.NormalizationForm]::FormD)
    $sb = New-Object System.Text.StringBuilder
    foreach ($c in $n.ToCharArray()) {
        if ([Globalization.CharUnicodeInfo]::GetUnicodeCategory($c) -eq [Globalization.UnicodeCategory]::NonSpacingMark) { continue }
        if ([int]$c -lt 32 -or [int]$c -gt 126 -or $c -eq ';') { continue }
        [void]$sb.Append($c)
    }
    $s = $sb.ToString()
    if ($s.Length -gt 60) { $s = $s.Substring(0, 60) }
    return $s
}

function Get-Frame {
    $q = $script:Quota
    # Une valeur perimee par un refus d'acces n'est plus affichee ; un 429 la garde.
    $show = -not ($q.Error -and $q.Code -ne 429)
    $five = if ($show) { $q.Five } else { -1 }
    $week = if ($show) { $q.Week } else { -1 }
    return 'Q;{0};{1};{2};{3};{4};{5};{6};{7}' -f `
        (ConvertTo-LocalEpoch (Get-Date)), $five, (ConvertTo-LocalEpoch $q.FiveReset),
        $week, (ConvertTo-LocalEpoch $q.WeekReset), $script:Level,
        (ConvertTo-LocalEpoch $q.Updated), (ConvertTo-Ascii $q.Error)
}

function Get-ElevenFrame {
    $e = $script:Eleven
    return 'E;{0};{1};{2};{3};{4}' -f $e.Used, $e.Limit, (ConvertTo-LocalEpoch $e.Reset),
        (ConvertTo-LocalEpoch $e.Updated), (ConvertTo-Ascii $e.Error)
}

# ---------------- Port serie ----------------
function Find-BoardPort {
    if ($Port) { return $Port }
    # USB Serial/JTAG natif de l'ESP32-C6 : VID 303A, PID 1001, interface 00.
    $dev = Get-CimInstance Win32_PnPEntity -Filter "PNPDeviceID LIKE 'USB\\VID_303A%'" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' } | Select-Object -First 1
    if ($dev -and $dev.Name -match '\((COM\d+)\)') { return $Matches[1] }
    return $null
}

$Serial = $null

function Open-Board {
    $name = Find-BoardPort
    if (-not $name) { return $false }
    try {
        $sp = New-Object System.IO.Ports.SerialPort $name, 115200
        $sp.NewLine = "`n"
        $sp.WriteTimeout = 2000
        $sp.ReadTimeout = 200
        # DTR/RTS restent bas : sur l'USB Serial/JTAG, les basculer reinitialise la puce.
        $sp.DtrEnable = $false
        $sp.RtsEnable = $false
        $sp.Open()
        $script:Serial = $sp
        Write-Log "Carte connectee sur $name"
        return $true
    } catch {
        Write-Log "Ouverture de $name impossible : $($_.Exception.Message)"
        return $false
    }
}

function Close-Board {
    if ($script:Serial) {
        try { $script:Serial.Close() } catch { }
        try { $script:Serial.Dispose() } catch { }
        $script:Serial = $null
    }
}

function Send-Frame {
    if (-not $script:Serial -and -not (Open-Board)) { return $false }
    $frame = Get-Frame
    try {
        $script:Serial.DiscardInBuffer()
        $script:Serial.WriteLine($frame)
        Write-Log "-> $frame"
        if ($script:ElevenKey) {
            $ef = Get-ElevenFrame
            $script:Serial.WriteLine($ef)
            Write-Log "-> $ef"
        }
        if ($script:SummaryFrame) { $script:Serial.WriteLine($script:SummaryFrame) }
        if ($script:MarketFrame) { $script:Serial.WriteLine($script:MarketFrame) }
        return $true
    } catch {
        Write-Log "Envoi impossible : $($_.Exception.Message)"
        Close-Board
        return $false
    }
}

# ---------------- Boucle ----------------
Write-Log 'MiniDisplay Bridge demarre'
Write-Log $(if ($ElevenKey) { 'ElevenLabs : cle trouvee, suivi active' } else { 'ElevenLabs : pas de cle, suivi desactive' })
Write-Log $(if ($MarketSymbol) { "Bourse : suivi de $MarketSymbol" } else { 'Bourse : pas de MARKET_SYMBOL, suivi desactive' })
Write-Log $(if ($SiamOn) { 'PALais SIAM : compte trouve, suivi des commandes active' } else { 'PALais SIAM : pas de compte, suivi desactive' })

$NextSend = Get-Date
try {
    while ($true) {
        $now = Get-Date
        $changed = $false
        if ($now -ge $NextStatus) { Update-Status; $changed = $true }
        if ($ElevenKey -and $now -ge $NextEleven) {
            Update-Eleven; $changed = $true
            if ($Eleven.Error) { Write-Log "ElevenLabs : $($Eleven.Error)" }
            else { Write-Log ("ElevenLabs : {0} / {1} credits" -f $Eleven.Used, $Eleven.Limit) }
        }
        if ($now -ge $NextQuota)  {
            Update-Quota; $changed = $true
            if ($Quota.Error) { Write-Log "Quota : $($Quota.Error)" }
            else { Write-Log ("Quota : 5 h {0} %, semaine {1} %" -f $Quota.Five, $Quota.Week) }
        }
        if ($SiamOn -and $now -ge $NextOrder) { Update-Orders }
        if ($SiamOn -and (Get-Date) -ge $NextSummary) { Update-Summary }
        if ($MarketSymbol -and (Get-Date) -ge $NextMarket) { Update-Market }
        if ($now -ge $NextPc) { Send-Pc; $NextPc = (Get-Date).AddSeconds($PcSeconds) }
        if ($changed -or $now -ge $NextSend) {
            $ok = Send-Frame
            $NextSend = $now.AddSeconds($(if ($ok) { $SendEverySeconds } else { 5 }))
            if ($Once) { break }
        }
        Start-Sleep -Milliseconds 500
    }
} finally {
    Close-Board
}
