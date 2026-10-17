import { Composition } from 'remotion';
import { MDMA, DURATION } from './MDMA';

export const Root = () => (
  <Composition id="MDMA" component={MDMA} durationInFrames={DURATION} fps={30} width={1920} height={1080} />
);
