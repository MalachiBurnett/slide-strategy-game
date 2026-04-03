import React from 'react';
import { motion } from 'motion/react';
import { ArrowLeft, Github, Briefcase, Palette, Users, TestTube } from 'lucide-react';

interface CreditsViewProps {
  onBack: () => void;
}

export const CreditsView: React.FC<CreditsViewProps> = ({ onBack }) => {
  const credits = [
    {
      name: 'Malachi Burnett',
      role: 'Developer',
      description: 'Sole developer of the site',
      icon: <Briefcase className="w-6 h-6 text-[var(--primaryText)]" />,
      link: 'https://portfolio.wiizardsoftware.uk',
      linkText: 'Portfolio'
    },
    {
      name: 'Oscar Lowther',
      role: 'John slide',
      description: 'What can i say, he is just John slide',
      icon: <Github className="w-6 h-6 text-[var(--primaryText)]" />,
      link: null,
      linkText: null
    },
    {
      name: 'Reuben Storer, Jo Sheehy, Ethan Taylor, and Leo Berner',
      role: 'Features Lead',
      description: 'The people contantly suggesting new features and improvements.',
      icon: <Github className="w-6 h-6 text-[var(--primaryText)]" />,
      link: null,
      linkText: null
    },
    {
      name: 'Asha Fitton-Patel',
      role: 'Game Designer',
      description: 'Original designer of the physical game (as far as i know, correct me if this game already existed)',
      icon: <Palette className="w-6 h-6 text-[var(--primaryText)]" />,
      link: null,
      linkText: null
    },
    {
      name: 'South Devon UTC Students',
      role: 'Beta Testers',
      description: 'Community provided extensive testing and feedback',
      icon: <Users className="w-6 h-6 text-[var(--primaryText)]" />,
      link: null,
      linkText: null
    },
  ];

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] p-4 sm:p-8 font-sans transition-colors duration-500">
      <div className="max-w-3xl mx-auto">
        <header className="flex items-center gap-6 mb-12">
          <button 
            onClick={onBack}
            className="p-3 bg-[var(--primary)] rounded-full shadow-lg hover:scale-110 transition-transform"
          >
            <ArrowLeft className="w-6 h-6 text-[var(--primaryText)]" />
          </button>
          <h1 className="text-4xl font-black">Credits</h1>
        </header>

        <div className="space-y-6">
          {credits.map((person, index) => (
            <motion.div
              key={index}
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: index * 0.1 }}
              className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-4 border-[var(--primary)] border-opacity-20 hover:border-opacity-50 transition-all"
            >
              <div className="flex items-start gap-6">
              <div className="p-4 bg-[var(--primary)] bg-opacity-10 rounded-2xl flex-shrink-0">
                  {person.icon}
                </div>
                <div className="flex-1 min-w-0">
                  <h2 className="text-2xl font-black mb-1">{person.name}</h2>
                  <p className="text-sm font-bold text-[var(--primary)] mb-2">{person.role}</p>
                  <p className="opacity-70 mb-4">{person.description}</p>
                  {person.link && (
                    <a 
                      href={person.link}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="inline-block px-6 py-2 bg-[var(--primary)] text-[var(--primaryText)] rounded-xl font-bold hover:opacity-90 transition-all shadow-md"
                    >
                      Visit {person.linkText}
                    </a>
                  )}
                </div>
              </div>
            </motion.div>
          ))}
        </div>

        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.4 }}
          className="mt-12 bg-[var(--bgLight)] p-8 rounded-3xl shadow-lg border-b-4 border-[var(--accent)] border-opacity-20 text-center"
        >
          <h3 className="text-xl font-bold mb-4 flex items-center justify-center gap-2">
            <TestTube className="w-5 h-5 text-[var(--accent)]" />
            Thank You!
          </h3>
          <p className="opacity-70">
            SLIDE was created with passion and brought to life through the collaboration of incredible individuals. 
            Thank you to everyone who contributed, tested, and believed in this project.
          </p>
        </motion.div>
      </div>
    </div>
  );
};
