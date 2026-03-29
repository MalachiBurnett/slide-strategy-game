export const EMOTES = [
  { id: 'facepalm', emoji: '🤦‍♂️', label: 'Facepalm' },
  { id: 'smile', emoji: '😁', label: 'Smile' },
  { id: 'angry', emoji: '😡', label: 'Angry' },
  { id: 'cursing', emoji: '🤬', label: 'Cursing' },
  { id: 'gg', emoji: 'GG', label: 'GG', isText: true }
];

export type EmoteId = typeof EMOTES[number]['id'];

export interface Emote {
  id: EmoteId;
  playerId: number;
  timestamp: number;
}
