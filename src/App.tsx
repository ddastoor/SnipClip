import React, { useMemo, useState } from 'react';
import releaseData from './data/releaseInfo.json';
import { Header } from './components/Header';
import { SnipStudio } from './components/SnipStudio';
import {
  DisplayDetectionPanel,
  SESSION_PRESETS,
  SessionPreset,
} from './components/DisplayDetectionPanel';
import { DebReleaseSection } from './components/DebReleaseSection';
import {
  detectLinuxDisplaySession,
  LinuxBinarySet,
  LinuxEnvInput,
} from './utils/snipEngine';

export function App() {
  const [activePresetId, setActivePresetId] = useState<string>(SESSION_PRESETS[0].id);
  const [env, setEnv] = useState<LinuxEnvInput>(SESSION_PRESETS[0].env);
  const [bins, setBins] = useState<LinuxBinarySet>(SESSION_PRESETS[0].bins);

  const detectedSession = useMemo(() => {
    return detectLinuxDisplaySession(env, bins);
  }, [env, bins]);

  const handleSelectPreset = (preset: SessionPreset) => {
    setActivePresetId(preset.id);
    setEnv(preset.env);
    setBins(preset.bins);
  };

  const handleCustomEnv = (nextEnv: LinuxEnvInput) => {
    setActivePresetId('custom');
    setEnv(nextEnv);
  };

  const handleCustomBins = (nextBins: LinuxBinarySet) => {
    setActivePresetId('custom');
    setBins(nextBins);
  };

  return (
    <div className="min-h-screen flex flex-col bg-slate-50 text-slate-900">
      <Header
        version={releaseData.version}
        debFilename={releaseData.debFilename}
        downloadPath={releaseData.downloadPath}
      />

      <main className="flex-1">
        <SnipStudio
          version={releaseData.version}
          debFilename={releaseData.debFilename}
          sessionInfo={detectedSession}
        />

        <DisplayDetectionPanel
          activePresetId={activePresetId}
          env={env}
          bins={bins}
          detected={detectedSession}
          onSelectPreset={handleSelectPreset}
          onChangeEnv={handleCustomEnv}
          onChangeBins={handleCustomBins}
        />

        <DebReleaseSection release={releaseData} />
      </main>

      <footer className="border-t border-slate-200 bg-white py-6">
        <div className="max-w-[1280px] mx-auto px-6 flex flex-col sm:flex-row items-center justify-between gap-4 text-xs text-slate-500">
          <div>
            SnipClip v{releaseData.version} · Windows Snipping Tool clone for Debian-based Linux
            distributions
          </div>
          <div className="flex items-center gap-4 font-mono tabular-nums">
            <a
              href={releaseData.downloadPath}
              download={releaseData.debFilename}
              className="text-slate-700 hover:text-slate-900 underline underline-offset-4"
            >
              {releaseData.debFilename}
            </a>
            <span aria-hidden="true">·</span>
            <span>Release {releaseData.releaseTag}</span>
          </div>
        </div>
      </footer>
    </div>
  );
}

export default App;
