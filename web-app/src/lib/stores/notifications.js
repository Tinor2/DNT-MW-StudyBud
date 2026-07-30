import { writable } from 'svelte/store';

function createNotificationStore() {
  const { subscribe, update } = writable([]);

  let nextId = 0;

  return {
    subscribe,
    add(type, feature, message) {
      const id = nextId++;
      const notification = {
        id,
        type,
        feature,
        message,
        timestamp: Date.now()
      };
      update(list => {
        const next = [...list, notification];
        if (next.length > 2) next.shift();
        return next;
      });
      setTimeout(() => {
        update(list => list.filter(n => n.id !== id));
      }, 4000);
    },
    dismiss(id) {
      update(list => list.filter(n => n.id !== id));
    },
    clear() {
      set([]);
    }
  };
}

export const notifications = createNotificationStore();
