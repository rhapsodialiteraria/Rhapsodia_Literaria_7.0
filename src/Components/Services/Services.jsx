import React, { useState } from 'react';
import './Services.css';
import theme_pattern from '../../assets/theme_pattern.svg';
import arrow_icon from '../../assets/arrow_icon.svg';
import service_data from '../../assets/services_data';

const Services = () => {
    const [selectedEvent, setSelectedEvent] = useState(null);

    const handleOpen = (event) => setSelectedEvent(event);
    const handleClose = () => setSelectedEvent(null);

    return (
        <div id="events" className="services">
            {/* === Main Title === */}
            <div className="services-title">
                <h1>Programs</h1>
                <img src={theme_pattern} alt="" />
            </div>

            {/* ---- Day 1 ---- */}
            <div id="day1" className="services-day">
                <div className="services-title">
                    <h1>Day 1</h1>
                </div>
                <div className="services-container">
                    {service_data.map((service, index) => (
                        <div
                            key={index}
                            className="services-format"
                            onClick={() => handleOpen(service)}
                        >
                            <h3>{service.s_no}</h3>
                            <h2>{service.s_name}</h2>
                            <p>{service.s_desc}</p>
                            <div className="services-readmore">
                                <p>Read More</p>
                                <img src={arrow_icon} alt="" />
                            </div>
                        </div>
                    ))}
                </div>
            </div>

            {/* ---- Day 2 ---- */}
            <div id="day2" className="services-day">
                <div className="services-title">
                    <h1>Day 2</h1>
                </div>
                <div className="services-container">
                    {service_data.map((service, index) => (
                        <div
                            key={index}
                            className="services-format"
                            onClick={() => handleOpen(service)}
                        >
                            <h3>{service.s_no}</h3>
                            <h2>{service.s_name}</h2>
                            <p>{service.s_desc}</p>
                            <div className="services-readmore">
                                <p>Read More</p>
                                <img src={arrow_icon} alt="" />
                            </div>
                        </div>
                    ))}
                </div>
            </div>

            {/* ---- Popup Modal ---- */}
            {selectedEvent && (
                <div className="modal-overlay" onClick={handleClose}>
                    <div
                        className="modal-content"
                        onClick={(e) => e.stopPropagation()}
                    >
                        <h2>{selectedEvent.s_name}</h2>
                        <p><strong>Premise:</strong> {selectedEvent.s_premise}</p>
                        <p><strong>Organizer:</strong> {selectedEvent.s_organizer}</p>
                        <p><strong>Prize:</strong> {selectedEvent.s_prize}</p>
                        <p>
                            <strong>Registration:</strong>{' '}
                            <a href={selectedEvent.s_link} target="_blank" rel="noreferrer">
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

export default Services;
