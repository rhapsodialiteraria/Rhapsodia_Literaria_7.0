import React, { useState } from 'react';
import './Day2Events.css';
import theme_pattern from '../../assets/theme_pattern.svg';
import arrow_icon from '../../assets/arrow_icon.svg';
import day2_data from '../../assets/day2_data';

const Day2Events = () => {
    const [selectedEvent, setSelectedEvent] = useState(null);

    const handleOpen = (event) => setSelectedEvent(event);
    const handleClose = () => setSelectedEvent(null);

    return (
        <div id="day2" className="events-section">
            <div className="events-title">
                <h1>Day 2-Events</h1>
                <img src={theme_pattern} alt="" />
            </div>

            <div className="events-container">
                {day2_data.map((event, index) => (
                    <div
                        key={index}
                        className="event-card"
                        onClick={() => handleOpen(event)}
                    >
                        <img src={event.e_poster} alt={event.e_title} className="event-poster" />
                    </div>
                ))}
            </div>

            {selectedEvent && (
                <div className="modal-overlay" onClick={handleClose}>
                    <div
                        className="modal-content"
                        onClick={(e) => e.stopPropagation()}
                    >
                        <h2>{selectedEvent.e_title}</h2>
                        <p><strong>Subtitle:</strong> {selectedEvent.e_subtitle}</p>
                        <p><strong>Coordinator:</strong> {selectedEvent.e_coordinator}</p>
                        <p><strong>Student Coordinators:</strong></p>
                        <ul>
                            {selectedEvent.e_student_coordinators.map((student, i) => (
                                <li key={i}>{student.name} – {student.contact}</li>
                            ))}
                        </ul>
                        <p>
                            <strong>Guidelines:</strong>{' '}
                            <a href={selectedEvent.e_guidelines} target="_blank" rel="noreferrer">
                                Click here
                            </a>
                        </p>
                        <p>
                            <strong>Registration:</strong>{' '}
                            <a href={selectedEvent.e_registration} target="_blank" rel="noreferrer">
                                Click here
                            </a>
                        </p>
                        <button className="close-btn" onClick={handleClose}>Close</button>
                    </div>
                </div>
            )}
        </div>
    );
};

export default Day2Events;
